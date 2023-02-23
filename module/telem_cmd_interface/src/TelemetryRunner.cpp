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
#include "WebsocketServer.h"
#include "ThreadNamer.h"

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
        void OnNewTelemetry(uint8_t* buffer, uint64_t bufferSize);
        void OnNewJsonTelemetry(const char* buffer, uint64_t bufferSize);
        void OnNewWebsocketConnectionCallback(struct mg_connection* conn);
        bool OnNewWebsocketDataReceivedCallback(struct mg_connection* conn, char* data, size_t data_len);

        volatile bool m_running;
        std::unique_ptr<boost::thread> m_threadPtr;
        std::unique_ptr<WebsocketServer> m_websocketServerPtr;
        std::unique_ptr<TelemetryLogger> m_telemetryLoggerPtr;
        DeadlineTimer m_deadlineTimer;
        HdtnConfig m_hdtnConfig;

        boost::mutex m_lastSerializedAllOutductCapabilitiesMutex;
        std::string m_lastSerializedAllOutductCapabilities;
};

/**
 * TelemetryRunner proxies 
 */
TelemetryRunner::TelemetryRunner()
    : m_pimpl(boost::make_unique<TelemetryRunner::Impl>())
{}

bool TelemetryRunner::Init(const HdtnConfig& hdtnConfig, zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions &options)
{
    return m_pimpl->Init(hdtnConfig, inprocContextPtr, options);
}

bool TelemetryRunner::ShouldExit()
{
    return m_pimpl->ShouldExit();
}

void TelemetryRunner::Stop()
{
    m_pimpl->Stop();
}

TelemetryRunner::~TelemetryRunner()
{
    Stop();
}

/**
 * TelemetryRunner implementation
 */

TelemetryRunner::Impl::Impl() :
    m_running(false),
    m_deadlineTimer(THREAD_POLL_INTERVAL_MS)
{

}

bool TelemetryRunner::Impl::Init(const HdtnConfig& hdtnConfig, zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions &options)
{
    if ((inprocContextPtr == NULL) && (!options.m_hdtnDistributedConfigPtr)) {
        LOG_ERROR(subprocess) << "Error in TelemetryRunner Init: using distributed mode but Hdtn Distributed Config is invalid";
        return false;
    }
    m_hdtnConfig = hdtnConfig;
#ifdef USE_WEB_INTERFACE
    m_websocketServerPtr = boost::make_unique<WebsocketServer>();
    m_websocketServerPtr->Init(options.m_guiDocumentRoot, options.m_guiPortNumber);
    m_websocketServerPtr->SetOnNewWebsocketConnectionCallback(
        boost::bind(&TelemetryRunner::Impl::OnNewWebsocketConnectionCallback, this, boost::placeholders::_1));
    m_websocketServerPtr->SetOnNewWebsocketDataReceivedCallback(
        boost::bind(&TelemetryRunner::Impl::OnNewWebsocketDataReceivedCallback, this,
            boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
#endif

#ifdef DO_STATS_LOGGING
    m_telemetryLoggerPtr = boost::make_unique<TelemetryLogger>();
#endif

    m_running = true;
    m_threadPtr = boost::make_unique<boost::thread>(
        boost::bind(&TelemetryRunner::Impl::ThreadFunc, this, options.m_hdtnDistributedConfigPtr, inprocContextPtr)); // create and start the worker thread
    return true;
}

void TelemetryRunner::Impl::OnNewWebsocketConnectionCallback(struct mg_connection* conn) {
    std::cout << "newconn\n";
    const std::string hdtnConfigSerialized = m_hdtnConfig.ToJson();
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, hdtnConfigSerialized.data(), hdtnConfigSerialized.size());
    {
        boost::mutex::scoped_lock lock(m_lastSerializedAllOutductCapabilitiesMutex);
        if (m_lastSerializedAllOutductCapabilities.size()) {
            mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, m_lastSerializedAllOutductCapabilities.data(), m_lastSerializedAllOutductCapabilities.size());
        }
    }
}
bool TelemetryRunner::Impl::OnNewWebsocketDataReceivedCallback(struct mg_connection* conn, char* data, size_t data_len) {
    std::cout << "newdata\n";
    return true;
}

void TelemetryRunner::Impl::ThreadFunc(const HdtnDistributedConfig_ptr& hdtnDistributedConfigPtr, zmq::context_t *inprocContextPtr)
{
    ThreadNamer::SetThisThreadName("TelemetryRunner");
    // Create and initialize connections
    std::unique_ptr<TelemetryConnection> ingressConnection;
    std::unique_ptr<TelemetryConnection> egressConnection;
    std::unique_ptr<TelemetryConnection> storageConnection;
    try
    {
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
    catch (std::exception& e)
    {
        LOG_ERROR(subprocess) << e.what();
        return;
    }

    // Create poller and add each connection
    TelemetryConnectionPoller poller;
    poller.AddConnection(*ingressConnection);
    poller.AddConnection(*egressConnection);
    poller.AddConnection(*storageConnection);

    // Start loop to begin polling
    
    while (m_running)
    {
        if (!m_deadlineTimer.SleepUntilNextInterval()) {
            break;
        }

        // Send signals to all hdtn modules
        static const zmq::const_buffer byteSignalBuf(&TELEM_REQ_MSG, sizeof(TELEM_REQ_MSG));
        ingressConnection->SendZmqConstBufferMessage(byteSignalBuf);
        egressConnection->SendZmqConstBufferMessage(byteSignalBuf);
        storageConnection->SendZmqConstBufferMessage(byteSignalBuf);

        // Wait for telemetry from all modules
        unsigned int receiveEventsMask = 0;
        for (unsigned int attempt = 0; attempt < NUM_POLL_ATTEMPTS; ++attempt)
        {
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
            }
            if (poller.HasNewMessage(*egressConnection)) {
                receiveEventsMask |= REC_EGRESS;
                zmq::message_t msg = egressConnection->ReadMessage();
                OnNewTelemetry((uint8_t*)msg.data(), msg.size());
                zmq::message_t msgJson = egressConnection->ReadMessage();
                OnNewJsonTelemetry((const char*)msgJson.data(), msgJson.size());
            }
            if (poller.HasNewMessage(*storageConnection)) {
                receiveEventsMask |= REC_STORAGE;
                zmq::message_t msgJson = storageConnection->ReadMessage();
                OnNewJsonTelemetry((const char*)msgJson.data(), msgJson.size());
            }
        }
        if (receiveEventsMask != REC_ALL) {
            LOG_WARNING(subprocess) << "did not get telemetry from all modules";
        }
    }
    LOG_DEBUG(subprocess) << "ThreadFunc exiting";
}

void TelemetryRunner::Impl::OnNewTelemetry(uint8_t* buffer, uint64_t bufferSize) {
    std::vector<std::unique_ptr<Telemetry_t> > telemList;
    try {
        telemList = TelemetryFactory::DeserializeFromLittleEndian(buffer, bufferSize);
    }
    catch (std::exception& e) {
        LOG_ERROR(subprocess) << e.what();
        return;
    }
    //std::cout << "telemListSize " << telemList.size() << "\n";
    for (std::unique_ptr<Telemetry_t>& telem : telemList) {
        if (telem->GetType() == TelemetryType::allOutductCapability) {
            //std::cout << telem->ToJson() << "\n";
            {
                boost::mutex::scoped_lock lock(m_lastSerializedAllOutductCapabilitiesMutex);
                m_lastSerializedAllOutductCapabilities = telem->ToJson();
            }
            if (m_websocketServerPtr) {
                m_websocketServerPtr->SendNewTextData(m_lastSerializedAllOutductCapabilities.data(), m_lastSerializedAllOutductCapabilities.size());
            }
        }
        if (m_telemetryLoggerPtr) {
            m_telemetryLoggerPtr->LogTelemetry(telem.get());
        }
    }

    if (m_websocketServerPtr) {
        m_websocketServerPtr->SendNewBinaryData((const char*)buffer, bufferSize);
    }
    
}

void TelemetryRunner::Impl::OnNewJsonTelemetry(const char* buffer, uint64_t bufferSize) {
    //printf("%.*s", (int)bufferSize, buffer); //not null terminated
    if (m_websocketServerPtr) {
        m_websocketServerPtr->SendNewTextData(buffer, bufferSize);
    }
}

bool TelemetryRunner::Impl::ShouldExit()
{
    if (m_websocketServerPtr) {
        return m_websocketServerPtr->RequestsExit();
    }
    return false;
}

void TelemetryRunner::Impl::Stop() {
    m_running = false;
    m_deadlineTimer.Disable();
    m_deadlineTimer.Cancel();
    if (!m_threadPtr) {
        return;
    }
    try {
        m_threadPtr->join();
    }
    catch (std::exception& e) {
        LOG_WARNING(subprocess) << e.what();
    }
    m_threadPtr.reset(); // delete it
    
    //The following is implicitly done at destruction.  If print statements are
    // added before and after this reset(), you will observe this part takes at least a second.
    // We may want to optimize this at some point.
    //m_websocketServerPtr.reset();
}
