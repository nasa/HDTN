/**
 * @file TelemetryRunner.cpp
 *
 * @copyright Copyright © 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 */

#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/make_unique.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/tokenizer.hpp>

#include "TelemetryRunner.h"
#include "Logger.h"
#include "SignalHandler.h"
#include "TelemetryDefinitions.h"
#include "TelemetryConnection.h"
#include "TelemetryConnectionPoller.h"
#include "TelemetryLogger.h"
#include "Environment.h"
#include "DeadlineTimer.h"
#include "BeastWebsocketServer.h"
#include "ThreadNamer.h"
#include <queue>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::telem;

/**
 * Polling options
 */
static const uint8_t NUM_POLL_ATTEMPTS = 4;
static const uint16_t THREAD_POLL_INTERVAL_MS = 1000;
static const uint16_t DEFAULT_BIG_TIMEOUT_POLL_MS = 250;

/**
 * Bitmask codes for tracking receive events
 */
static const unsigned int REC_ALL = 0x07;
static const unsigned int REC_INGRESS = 0x01;
static const unsigned int REC_EGRESS = 0x02;
static const unsigned int REC_STORAGE = 0x04;

static void CustomCleanupStdString(void* data, void* hint) {
    delete static_cast<std::string*>(hint);
}

/**
 * TelemetryRunner implementation class 
 */
class TelemetryRunner::Impl : private boost::noncopyable {
    public:
        Impl();
        bool Init(const HdtnConfig& hdtnConfig, zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions& options);
        bool ShouldExit();
        void Stop();

    private:
        void ThreadFunc(const HdtnDistributedConfig_ptr& hdtnDistributedConfigPtr, zmq::context_t * inprocContextPtr);
        void OnNewJsonTelemetry(const char* buffer, uint64_t bufferSize);
        void OnNewWebsocketConnectionCallback(WebsocketSessionBase& conn);
        bool OnNewWebsocketDataReceivedCallback(WebsocketSessionBase& conn, std::string& receivedString);

        volatile bool m_running;
        std::unique_ptr<boost::thread> m_threadPtr;
        std::unique_ptr<BeastWebsocketServer> m_websocketServerPtr;
        std::unique_ptr<TelemetryLogger> m_telemetryLoggerPtr;
        DeadlineTimer m_deadlineTimer;
        HdtnConfig m_hdtnConfig;

        boost::mutex m_lastSerializedAllOutductCapabilitiesMutex;
        std::shared_ptr<std::string> m_lastJsonSerializedAllOutductCapabilitiesPtr;

        std::queue<zmq::message_t> m_apiCallsToIngressQueue;
        boost::mutex m_apiCallsToIngressQueueMutex;

        std::queue<zmq::message_t> m_apiCallsToEgressQueue;
        boost::mutex m_apiCallsToEgressQueueMutex;

        std::queue<zmq::message_t> m_apiCallsToStorageQueue;
        boost::mutex m_apiCallsToStorageQueueMutex;
};

/**
 * TelemetryRunner proxies 
 */
TelemetryRunner::TelemetryRunner()
    : m_pimpl(boost::make_unique<TelemetryRunner::Impl>())
{}

bool TelemetryRunner::Init(const HdtnConfig& hdtnConfig, zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions &options) {
    return m_pimpl->Init(hdtnConfig, inprocContextPtr, options);
}

bool TelemetryRunner::ShouldExit() {
    return m_pimpl->ShouldExit();
}

void TelemetryRunner::Stop() {
    m_pimpl->Stop();
}

TelemetryRunner::~TelemetryRunner() {
    Stop();
}

/**
 * TelemetryRunner implementation
 */

TelemetryRunner::Impl::Impl() :
    m_running(false),
    m_deadlineTimer(THREAD_POLL_INTERVAL_MS) {}

bool TelemetryRunner::Impl::Init(const HdtnConfig& hdtnConfig, zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions &options) {
    if ((inprocContextPtr == NULL) && (!options.m_hdtnDistributedConfigPtr)) {
        LOG_ERROR(subprocess) << "Error in TelemetryRunner Init: using distributed mode but Hdtn Distributed Config is invalid";
        return false;
    }
    m_hdtnConfig = hdtnConfig;
#ifdef USE_WEB_INTERFACE
    m_websocketServerPtr = boost::make_unique<BeastWebsocketServer>();
    m_websocketServerPtr->Init(options.m_guiDocumentRoot, options.m_guiPortNumber,
        boost::bind(&TelemetryRunner::Impl::OnNewWebsocketConnectionCallback, this, boost::placeholders::_1),
        boost::bind(&TelemetryRunner::Impl::OnNewWebsocketDataReceivedCallback, this,
            boost::placeholders::_1, boost::placeholders::_2));
#endif

#ifdef DO_STATS_LOGGING
    m_telemetryLoggerPtr = boost::make_unique<TelemetryLogger>();
#endif

    m_running = true;
    m_threadPtr = boost::make_unique<boost::thread>(
        boost::bind(&TelemetryRunner::Impl::ThreadFunc, this, options.m_hdtnDistributedConfigPtr, inprocContextPtr)); // create and start the worker thread
    return true;
}

void TelemetryRunner::Impl::OnNewWebsocketConnectionCallback(WebsocketSessionBase& conn) {
    //std::cout << "newconn\n";
    conn.AsyncSendTextData(std::make_shared<std::string>(m_hdtnConfig.ToJson()));
    {
        boost::mutex::scoped_lock lock(m_lastSerializedAllOutductCapabilitiesMutex);
        if (m_lastJsonSerializedAllOutductCapabilitiesPtr->size()) {
            conn.AsyncSendTextData(std::shared_ptr<std::string>(m_lastJsonSerializedAllOutductCapabilitiesPtr)); //create copy of shared ptr and move the copy in
        }
    }
}
bool TelemetryRunner::Impl::OnNewWebsocketDataReceivedCallback(WebsocketSessionBase& conn, std::string& receivedString) {
    //std::cout << "newdata " << receivedString << "\n";
    boost::property_tree::ptree pt;
    if (JsonSerializable::GetPropertyTreeFromJsonString(receivedString, pt)) {
        std::string apiCall;
        try {
            apiCall = pt.get<std::string>("apiCall");
        }
        catch (const boost::property_tree::ptree_error& e) {
            LOG_ERROR(subprocess) << "parsing JSON OnNewWebsocketDataReceivedCallback: " << e.what();
            return true; //keep open
        }
        //LOG_INFO(subprocess) << "got api call " << apiCall;
        std::string* apiCmdStr = new std::string(std::move(receivedString));
        std::string& strRef = *apiCmdStr;
        if (apiCall == "ping") {
            boost::mutex::scoped_lock lock(m_apiCallsToIngressQueueMutex);
            m_apiCallsToIngressQueue.emplace(&strRef[0], strRef.size(), CustomCleanupStdString, apiCmdStr);
        }
        else {
            delete apiCmdStr;
        }
    }
    return true;
}

void TelemetryRunner::Impl::ThreadFunc(const HdtnDistributedConfig_ptr& hdtnDistributedConfigPtr, zmq::context_t *inprocContextPtr) {
    ThreadNamer::SetThisThreadName("TelemetryRunner");
    // Create and initialize connections
    std::unique_ptr<TelemetryConnection> ingressConnection;
    std::unique_ptr<TelemetryConnection> egressConnection;
    std::unique_ptr<TelemetryConnection> storageConnection;
    try {
        if (inprocContextPtr) {
            ingressConnection = boost::make_unique<TelemetryConnection>(
                "inproc://connecting_telem_to_from_bound_ingress",
                inprocContextPtr);
            egressConnection = boost::make_unique<TelemetryConnection>(
                "inproc://connecting_telem_to_from_bound_egress",
                inprocContextPtr);
            storageConnection = boost::make_unique<TelemetryConnection>(
                "inproc://connecting_telem_to_from_bound_storage",
                inprocContextPtr);
        }
        else {
            const std::string connect_connectingTelemToFromBoundIngressPath(
                std::string("tcp://") +
                hdtnDistributedConfigPtr->m_zmqIngressAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnDistributedConfigPtr->m_zmqConnectingTelemToFromBoundIngressPortPath));
            const std::string connect_connectingTelemToFromBoundEgressPath(
                std::string("tcp://") +
                hdtnDistributedConfigPtr->m_zmqEgressAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnDistributedConfigPtr->m_zmqConnectingTelemToFromBoundEgressPortPath));
            const std::string connect_connectingTelemToFromBoundStoragePath(
                std::string("tcp://") +
                hdtnDistributedConfigPtr->m_zmqStorageAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnDistributedConfigPtr->m_zmqConnectingTelemToFromBoundStoragePortPath));

            ingressConnection = boost::make_unique<TelemetryConnection>(connect_connectingTelemToFromBoundIngressPath, nullptr);
            egressConnection = boost::make_unique<TelemetryConnection>(connect_connectingTelemToFromBoundEgressPath, nullptr);
            storageConnection = boost::make_unique<TelemetryConnection>(connect_connectingTelemToFromBoundStoragePath, nullptr);
        }
    }
    catch (std::exception& e) {
        LOG_ERROR(subprocess) << e.what();
        return;
    }

    // Create poller and add each connection
    TelemetryConnectionPoller poller;
    poller.AddConnection(*ingressConnection);
    poller.AddConnection(*egressConnection);
    poller.AddConnection(*storageConnection);

    // Start loop to begin polling
    
    while (m_running) {
        if (!m_deadlineTimer.SleepUntilNextInterval()) {
            break;
        }

        // Send signals to all hdtn modules
        static const zmq::const_buffer byteSignalBuf(&TELEM_REQ_MSG, sizeof(TELEM_REQ_MSG));
        static const zmq::const_buffer byteSignalBufPlusApi(&TELEM_REQ_MSG_PLUS_API_CALLS, sizeof(TELEM_REQ_MSG_PLUS_API_CALLS));
        {
            boost::mutex::scoped_lock lock(m_apiCallsToIngressQueueMutex);
            if (m_apiCallsToIngressQueue.empty()) {
                ingressConnection->SendZmqConstBufferMessage(byteSignalBuf, false);
            }
            else {
                ingressConnection->SendZmqConstBufferMessage(byteSignalBufPlusApi, true);
            }
            while (!m_apiCallsToIngressQueue.empty()) {
                const bool moreFlag = (m_apiCallsToIngressQueue.size() > 1);
                ingressConnection->SendZmqMessage(std::move(m_apiCallsToIngressQueue.front()), moreFlag);
                m_apiCallsToIngressQueue.pop();
            }
        }
        egressConnection->SendZmqConstBufferMessage(byteSignalBuf, false);
        storageConnection->SendZmqConstBufferMessage(byteSignalBuf, false);

        // Wait for telemetry from all modules
        unsigned int receiveEventsMask = 0;
        for (unsigned int attempt = 0; attempt < NUM_POLL_ATTEMPTS; ++attempt) {
            if (receiveEventsMask == REC_ALL) {
                break;
            }

            if (!poller.PollConnections(DEFAULT_BIG_TIMEOUT_POLL_MS)) {
                continue;
            }

            if (poller.HasNewMessage(*ingressConnection)) {
                receiveEventsMask |= REC_INGRESS;
                zmq::message_t msgJson = ingressConnection->ReadMessage();
                OnNewJsonTelemetry((const char*)msgJson.data(), msgJson.size());
                if (m_telemetryLoggerPtr) {
                    AllInductTelemetry_t t;
                    if (t.SetValuesFromJsonCharArray((const char*)msgJson.data(), msgJson.size())) {
                        m_telemetryLoggerPtr->LogTelemetry(&t);
                    }
                    else {
                        LOG_ERROR(subprocess) << "cannot deserialize AllInductTelemetry_t for TelemetryLogger";
                    }
                }
            }
            if (poller.HasNewMessage(*egressConnection)) {
                receiveEventsMask |= REC_EGRESS;
                zmq::message_t msg = egressConnection->ReadMessage();
                zmq::message_t msg2;
                const zmq::message_t* messageOutductTelemPtr = &msg;
                OnNewJsonTelemetry((const char*)msg.data(), msg.size());
                if (msg.more()) { //msg was Aoct type, msg2 is normal ouduct telem
                    {
                        boost::mutex::scoped_lock lock(m_lastSerializedAllOutductCapabilitiesMutex);
                        m_lastJsonSerializedAllOutductCapabilitiesPtr = std::make_shared<std::string>(msg.to_string());
                    }
                    if (m_websocketServerPtr) {
                        m_websocketServerPtr->SendTextDataToActiveWebsockets(m_lastJsonSerializedAllOutductCapabilitiesPtr);
                    }
                    msg2 = egressConnection->ReadMessage();
                    OnNewJsonTelemetry((const char*)msg2.data(), msg2.size());
                    messageOutductTelemPtr = &msg2;
                }
                if (m_telemetryLoggerPtr) {
                    AllOutductTelemetry_t t;
                    if (t.SetValuesFromJsonCharArray((const char*)messageOutductTelemPtr->data(), messageOutductTelemPtr->size())) {
                        m_telemetryLoggerPtr->LogTelemetry(&t);
                    }
                    else {
                        LOG_ERROR(subprocess) << "cannot deserialize AllOutductTelemetry_t for TelemetryLogger";
                    }
                }
            }
            if (poller.HasNewMessage(*storageConnection)) {
                receiveEventsMask |= REC_STORAGE;
                zmq::message_t msgJson = storageConnection->ReadMessage();
                OnNewJsonTelemetry((const char*)msgJson.data(), msgJson.size());
                if (m_telemetryLoggerPtr) {
                    StorageTelemetry_t t;
                    if (t.SetValuesFromJsonCharArray((const char*)msgJson.data(), msgJson.size())) {
                        m_telemetryLoggerPtr->LogTelemetry(&t);
                    }
                    else {
                        LOG_ERROR(subprocess) << "cannot deserialize StorageTelemetry_t for TelemetryLogger";
                    }
                }
            }
        }
        if (receiveEventsMask != REC_ALL) {
            LOG_WARNING(subprocess) << "did not get telemetry from all modules";
        }
    }
    LOG_DEBUG(subprocess) << "ThreadFunc exiting";
}


void TelemetryRunner::Impl::OnNewJsonTelemetry(const char* buffer, uint64_t bufferSize) {
    //printf("%.*s", (int)bufferSize, buffer); //not null terminated
    if (m_websocketServerPtr) {
        std::shared_ptr<std::string> strPtr = std::make_shared<std::string>(buffer, bufferSize);
        m_websocketServerPtr->SendTextDataToActiveWebsockets(strPtr);
    }
}

bool TelemetryRunner::Impl::ShouldExit() {
    //websocket server does not support this
    return false;
}

void TelemetryRunner::Impl::Stop() {
    m_running = false;
    m_deadlineTimer.Disable();
    m_deadlineTimer.Cancel();
    if (m_threadPtr) {
        try {
            m_threadPtr->join();
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping TelemetryRunner thread";
        }
        m_threadPtr.reset(); // delete it
    }
    //stop websocket after thread
    if (m_websocketServerPtr) {
        m_websocketServerPtr->Stop();
        m_websocketServerPtr.reset();
    }
}
