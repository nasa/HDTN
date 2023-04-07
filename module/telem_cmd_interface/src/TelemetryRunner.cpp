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
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/make_unique.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/tokenizer.hpp>
#include <boost/asio.hpp>

#include "TelemetryRunner.h"
#include "Logger.h"
#include "SignalHandler.h"
#include "TelemetryDefinitions.h"
#include "TelemetryConnection.h"
#include "TelemetryConnectionPoller.h"
#include "TelemetryLogger.h"
#include "Environment.h"
#include "DeadlineTimer.h"
#include "ThreadNamer.h"
#include <queue>
#ifdef USE_WEB_INTERFACE
#include "BeastWebsocketServer.h"
#endif

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
static const unsigned int REC_INGRESS = 0x01;
static const unsigned int REC_EGRESS = 0x02;
static const unsigned int REC_STORAGE = 0x04;
static const unsigned int REC_API = 0x08;


/**
 * TelemetryRunner implementation class 
 */
class TelemetryRunner::Impl : private boost::noncopyable {
    public:
        Impl();
        bool Init(const HdtnConfig& hdtnConfig, zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions& options);
        void Stop();

    private:
        void ThreadFunc(const HdtnDistributedConfig_ptr& hdtnDistributedConfigPtr, zmq::context_t * inprocContextPtr);
        void OnNewJsonTelemetry(const char* buffer, uint64_t bufferSize);
        void OnNewWebsocketConnectionCallback(WebsocketSessionPublicBase& conn);
        bool OnNewWebsocketDataReceivedCallback(WebsocketSessionPublicBase& conn, std::string& receivedString);
        bool OnApiRequest(std::string&& msgJson, ApiSource_t src);
        bool HandlePingCommand(std::string& movablePayload, ApiSource_t src);
        bool HandleUploadContactPlanCommand(std::string& movablePayload, ApiSource_t src);
        bool HandleGetExpiringStorageCommand(std::string& movablePayload, ApiSource_t src);

        volatile bool m_running;
        std::unique_ptr<boost::thread> m_threadPtr;
#ifdef USE_WEB_INTERFACE
        std::unique_ptr<BeastWebsocketServer> m_websocketServerPtr;
#endif
        std::unique_ptr<TelemetryLogger> m_telemetryLoggerPtr;
        DeadlineTimer m_deadlineTimer;
        HdtnConfig m_hdtnConfig;
        std::shared_ptr<std::string> m_hdtnConfigJsonPtr;

        boost::mutex m_lastSerializedAllOutductCapabilitiesMutex;
        std::shared_ptr<std::string> m_lastJsonSerializedAllOutductCapabilitiesPtr;

        std::unique_ptr<TelemetryConnection> m_ingressConnection;
        std::unique_ptr<TelemetryConnection> m_egressConnection;
        std::unique_ptr<TelemetryConnection> m_storageConnection;
        std::unique_ptr<TelemetryConnection> m_schedulerConnection;
        std::unique_ptr<TelemetryConnection> m_apiConnection;

        typedef boost::function<bool (std::string& movablePayload, ApiSource_t src)> ApiCommandFunction_t;
        typedef std::unordered_map<std::string, ApiCommandFunction_t> ApiCommandFunctionMap_t;
        ApiCommandFunctionMap_t m_apiCmdMap;
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
    m_deadlineTimer(THREAD_POLL_INTERVAL_MS)
{
    m_apiCmdMap["ping"] = boost::bind(&TelemetryRunner::Impl::HandlePingCommand, this, boost::placeholders::_1, boost::placeholders::_2);
    m_apiCmdMap["upload_contact_plan"] = boost::bind(&TelemetryRunner::Impl::HandleUploadContactPlanCommand, this, boost::placeholders::_1, boost::placeholders::_2);
    m_apiCmdMap["get_expiring_storage"] = boost::bind(&TelemetryRunner::Impl::HandleGetExpiringStorageCommand, this, boost::placeholders::_1, boost::placeholders::_2);
}

bool TelemetryRunner::Impl::Init(const HdtnConfig& hdtnConfig, zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions &options) {
    if ((inprocContextPtr == NULL) && (!options.m_hdtnDistributedConfigPtr)) {
        LOG_ERROR(subprocess) << "Error in TelemetryRunner Init: using distributed mode but Hdtn Distributed Config is invalid";
        return false;
    }
    m_hdtnConfig = hdtnConfig;
    { //add hdtn version to config, and preserialize it to json once for all connecting web GUIs
        boost::property_tree::ptree pt = hdtnConfig.GetNewPropertyTree();
        pt.put("hdtnVersionString", hdtn::Logger::GetHdtnVersionAsString());
        m_hdtnConfigJsonPtr = std::make_shared<std::string>(JsonSerializable::PtToJsonString(pt));
    }
    
#ifdef USE_WEB_INTERFACE
# ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
    boost::asio::ssl::context sslContext(boost::asio::ssl::context::sslv23_server);
    if (options.m_sslPaths.m_valid) {
        try {
            //tcpclv4 server supports tls 1.2 and 1.3 only
            sslContext.set_options(
                boost::asio::ssl::context::default_workarounds
                | boost::asio::ssl::context::no_sslv2
                | boost::asio::ssl::context::no_sslv3
                | boost::asio::ssl::context::no_tlsv1
                | boost::asio::ssl::context::no_tlsv1_1
                | boost::asio::ssl::context::single_dh_use);
            if (options.m_sslPaths.m_certificateChainPemFile.size()) {
                sslContext.use_certificate_chain_file(options.m_sslPaths.m_certificateChainPemFile.string());
            }
            else {
                sslContext.use_certificate_file(options.m_sslPaths.m_certificatePemFile.string(), boost::asio::ssl::context::pem);
            }
            sslContext.use_private_key_file(options.m_sslPaths.m_privateKeyPemFile.string(), boost::asio::ssl::context::pem);
            sslContext.use_tmp_dh_file(options.m_sslPaths.m_diffieHellmanParametersPemFile.string()); //"C:/hdtn_ssl_certificates/dh4096.pem"
        }
        catch (boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "SSL error in TelemetryRunner Init: " << e.what();
            return false;
        }
    }
    m_websocketServerPtr = boost::make_unique<BeastWebsocketServer>(std::move(sslContext), options.m_sslPaths.m_valid);
# else
    m_websocketServerPtr = boost::make_unique<BeastWebsocketServer>();
# endif //#ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
    m_websocketServerPtr->Init(options.m_guiDocumentRoot, options.m_guiPortNumber,
        boost::bind(&TelemetryRunner::Impl::OnNewWebsocketConnectionCallback, this, boost::placeholders::_1),
        boost::bind(&TelemetryRunner::Impl::OnNewWebsocketDataReceivedCallback, this,
            boost::placeholders::_1, boost::placeholders::_2));
#endif //USE_WEB_INTERFACE

#ifdef DO_STATS_LOGGING
    m_telemetryLoggerPtr = boost::make_unique<TelemetryLogger>();
#endif

    m_running = true;
    m_threadPtr = boost::make_unique<boost::thread>(
        boost::bind(&TelemetryRunner::Impl::ThreadFunc, this, options.m_hdtnDistributedConfigPtr, inprocContextPtr)); // create and start the worker thread
    return true;
}

void TelemetryRunner::Impl::OnNewWebsocketConnectionCallback(WebsocketSessionPublicBase& conn) {
    //std::cout << "newconn\n";
    conn.AsyncSendTextData(std::shared_ptr<std::string>(m_hdtnConfigJsonPtr));
    {
        boost::mutex::scoped_lock lock(m_lastSerializedAllOutductCapabilitiesMutex);
        if (m_lastJsonSerializedAllOutductCapabilitiesPtr && m_lastJsonSerializedAllOutductCapabilitiesPtr->size()) {
            conn.AsyncSendTextData(std::shared_ptr<std::string>(m_lastJsonSerializedAllOutductCapabilitiesPtr)); //create copy of shared ptr and move the copy in
        }
    }
}

bool TelemetryRunner::Impl::OnNewWebsocketDataReceivedCallback(WebsocketSessionPublicBase& conn, std::string& receivedString) {
    if (!OnApiRequest(std::move(receivedString), ApiSource_t::webgui)) {
        LOG_ERROR(subprocess) << "failed to handle API request from websocket";
    }
    return true; // keep open
}

bool TelemetryRunner::Impl::HandlePingCommand(std::string& movablePayload, ApiSource_t src) {
    return m_ingressConnection->EnqueueApiPayload(std::move(movablePayload), src);
}

bool TelemetryRunner::Impl::HandleUploadContactPlanCommand(std::string& movablePayload, ApiSource_t src) {
    return m_schedulerConnection->EnqueueApiPayload(std::move(movablePayload), src);
}

bool TelemetryRunner::Impl::HandleGetExpiringStorageCommand(std::string& movablePayload, ApiSource_t src) {
    return m_storageConnection->EnqueueApiPayload(std::move(movablePayload), src);
}

bool TelemetryRunner::Impl::OnApiRequest(std::string&& msgJson, ApiSource_t src) {
    std::string apiCall = ApiCommand_t::GetApiCallFromJson(msgJson);
    TelemetryRunner::Impl::ApiCommandFunctionMap_t::iterator it = m_apiCmdMap.find(apiCall);
    if (it == m_apiCmdMap.end()) {
        LOG_ERROR(subprocess) << "Unrecognized API command " << apiCall;
        return false;
    }
    return it->second(msgJson, src); //note: msgJson will still be moved (boost::function doesn't support r-value references as parameters)
}

static bool ReceivedApi(unsigned int mask) {
    return (mask & REC_API);
}

static bool ReceivedIngress(unsigned int mask) {
    return (mask & REC_INGRESS);
}

static bool ReceivedEgress(unsigned int mask) {
    return (mask & REC_EGRESS);
}

static bool ReceivedStorage(unsigned int mask) {
    return (mask & REC_STORAGE);
}

static bool ReceivedAllRequired(unsigned int mask) {
    return ReceivedStorage(mask) && ReceivedEgress(mask) && ReceivedIngress(mask);
}

void TelemetryRunner::Impl::ThreadFunc(const HdtnDistributedConfig_ptr& hdtnDistributedConfigPtr, zmq::context_t *inprocContextPtr) {
    ThreadNamer::SetThisThreadName("TelemetryRunner");
    // Create and initialize connections
    try {
        if (inprocContextPtr) {
            m_ingressConnection = boost::make_unique<TelemetryConnection>(
                "inproc://connecting_telem_to_from_bound_ingress",
                inprocContextPtr);
            m_egressConnection = boost::make_unique<TelemetryConnection>(
                "inproc://connecting_telem_to_from_bound_egress",
                inprocContextPtr);
            m_storageConnection = boost::make_unique<TelemetryConnection>(
                "inproc://connecting_telem_to_from_bound_storage",
                inprocContextPtr);
            m_schedulerConnection = boost::make_unique<TelemetryConnection>(
                "inproc://connecting_telem_to_from_bound_scheduler",
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
            const std::string connect_connectingTelemToFromBoundSchedulerPath(
                std::string("tcp://") +
                hdtnDistributedConfigPtr->m_zmqSchedulerAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnDistributedConfigPtr->m_zmqConnectingTelemToFromBoundSchedulerPortPath));

            m_ingressConnection = boost::make_unique<TelemetryConnection>(connect_connectingTelemToFromBoundIngressPath, nullptr);
            m_egressConnection = boost::make_unique<TelemetryConnection>(connect_connectingTelemToFromBoundEgressPath, nullptr);
            m_storageConnection = boost::make_unique<TelemetryConnection>(connect_connectingTelemToFromBoundStoragePath, nullptr);
            m_schedulerConnection = boost::make_unique<TelemetryConnection>(connect_connectingTelemToFromBoundSchedulerPath, nullptr);
        }

        const std::string connect_connectingTelemToApi(
                std::string("tcp://*:") +
                boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundTelemApiPortPath));
        m_apiConnection = boost::make_unique<TelemetryConnection>(
            connect_connectingTelemToApi,
            nullptr,
            true
        );
    }
    catch (std::exception& e) {
        LOG_ERROR(subprocess) << e.what();
        return;
    }

    // Create poller and add each connection
    TelemetryConnectionPoller poller;
    poller.AddConnection(*m_ingressConnection);
    poller.AddConnection(*m_egressConnection);
    poller.AddConnection(*m_storageConnection);
    poller.AddConnection(*m_schedulerConnection);
    poller.AddConnection(*m_apiConnection);

    // Start loop to begin polling
    
    while (m_running) {
        if (!m_deadlineTimer.SleepUntilNextInterval()) {
            break;
        }

        // Send signals to all hdtn modules
        m_storageConnection->SendRequest();
        m_egressConnection->SendRequest();
        m_ingressConnection->SendRequest();
        m_schedulerConnection->SendRequest(false);

        // Poll for telemetry from all modules
        // Poll for new API requests
        unsigned int receiveEventsMask = 0;
        AllInductTelemetry_t inductTelem;
        AllOutductTelemetry_t outductTelem;
        StorageTelemetry_t storageTelem;
        for (unsigned int attempt = 0; attempt < NUM_POLL_ATTEMPTS; ++attempt) {
            if (ReceivedAllRequired(receiveEventsMask)) {
                break;
            }

            if (!poller.PollConnections(DEFAULT_BIG_TIMEOUT_POLL_MS)) {
                continue;
            }

            if (poller.HasNewMessage(*m_ingressConnection)) {
                receiveEventsMask |= REC_INGRESS;
                zmq::message_t msgJson = m_ingressConnection->ReadMessage();
                OnNewJsonTelemetry((const char*)msgJson.data(), msgJson.size());
                if (!inductTelem.SetValuesFromJsonCharArray((const char*)msgJson.data(), msgJson.size())) {
                    LOG_ERROR(subprocess) << "cannot deserialize AllInductTelemetry_t for TelemetryLogger";
                }
                if (m_ingressConnection->m_apiSocketAwaitingResponse) {
                    m_ingressConnection->m_apiSocketAwaitingResponse = false;
                    zmq::message_t msg;
                    m_apiConnection->SendZmqMessage(std::move(msg), false);
                }
            }
            if (poller.HasNewMessage(*m_egressConnection)) {
                receiveEventsMask |= REC_EGRESS;
                zmq::message_t msg = m_egressConnection->ReadMessage();
                zmq::message_t msg2;
                const zmq::message_t* messageOutductTelemPtr = &msg;
                OnNewJsonTelemetry((const char*)msg.data(), msg.size());
                if (msg.more()) { //msg was Aoct type, msg2 is normal ouduct telem
                    {
                        boost::mutex::scoped_lock lock(m_lastSerializedAllOutductCapabilitiesMutex);
                        m_lastJsonSerializedAllOutductCapabilitiesPtr = std::make_shared<std::string>(msg.to_string());
                    }
#ifdef USE_WEB_INTERFACE
                    if (m_websocketServerPtr) {
                        m_websocketServerPtr->SendTextDataToActiveWebsockets(m_lastJsonSerializedAllOutductCapabilitiesPtr);
                    }
#endif
                    msg2 = m_egressConnection->ReadMessage();
                    OnNewJsonTelemetry((const char*)msg2.data(), msg2.size());
                    messageOutductTelemPtr = &msg2;
                }
                if (!outductTelem.SetValuesFromJsonCharArray((const char*)messageOutductTelemPtr->data(), messageOutductTelemPtr->size())) {
                    LOG_ERROR(subprocess) << "cannot deserialize AllOutductTelemetry_t";
                }
            }
            if (poller.HasNewMessage(*m_storageConnection)) {
                receiveEventsMask |= REC_STORAGE;
                zmq::message_t msgJson = m_storageConnection->ReadMessage();
                zmq::message_t msg2;
                OnNewJsonTelemetry((const char*)msgJson.data(), msgJson.size());
                if (!storageTelem.SetValuesFromJsonCharArray((const char*)msgJson.data(), msgJson.size())) {
                    LOG_ERROR(subprocess) << "cannot deserialize StorageTelemetry_t";
                }
                const bool more = msgJson.more();
                if (more) {
                    StorageExpiringBeforeThresholdTelemetry_t storageExpiringTelem;
                    msg2 = m_storageConnection->ReadMessage();
                    if (!storageExpiringTelem.SetValuesFromJsonCharArray((const char*)msg2.data(), msg2.size())) {
                        LOG_ERROR(subprocess) << "cannot deserialize StorageExpiringBeforeThresholdTelemetry_t";
                    }
                }
                if (m_storageConnection->m_apiSocketAwaitingResponse && more) {
                    m_storageConnection->m_apiSocketAwaitingResponse = false;
                    m_apiConnection->SendZmqMessage(std::move(msgJson), true);
                    m_apiConnection->SendZmqMessage(std::move(msg2), false);
                }
            }
            if (poller.HasNewMessage(*m_schedulerConnection)) {
                // We only get this response if we sent an API command to scheduler.
                // Scheduler does not currently send data, so throw away the message content.
                m_schedulerConnection->ReadMessage();
                if (m_schedulerConnection->m_apiSocketAwaitingResponse) {
                    m_schedulerConnection->m_apiSocketAwaitingResponse = false;
                    zmq::message_t msg;
                    m_apiConnection->SendZmqMessage(std::move(msg), false);
                }
            }
            // Ensure only one API message is processed per request loop. This ensures clients
            // will receive the correct response when there are multiple connections.
            if (!ReceivedApi(receiveEventsMask) && poller.HasNewMessage(*m_apiConnection)) {
                receiveEventsMask |= REC_API;
                zmq::message_t msgJson = m_apiConnection->ReadMessage();
                OnApiRequest(msgJson.to_string(), ApiSource_t::socket);
            }
        }
        if (ReceivedAllRequired(receiveEventsMask)) {
            if (m_telemetryLoggerPtr) {
                m_telemetryLoggerPtr->LogTelemetry(inductTelem, outductTelem, storageTelem);
            }
        }
        else {
            LOG_WARNING(subprocess) << "did not get telemetry from all modules. missing:" <<
                (ReceivedEgress(receiveEventsMask) ? "" : " egress") <<
                (ReceivedIngress(receiveEventsMask) ? "" : " ingress") <<
                (ReceivedStorage(receiveEventsMask) ? "" : " storage");
        }
    }
    LOG_DEBUG(subprocess) << "ThreadFunc exiting";
}


void TelemetryRunner::Impl::OnNewJsonTelemetry(const char* buffer, uint64_t bufferSize) {
#ifdef USE_WEB_INTERFACE
    if (m_websocketServerPtr) {
        std::shared_ptr<std::string> strPtr = std::make_shared<std::string>(buffer, bufferSize);
        m_websocketServerPtr->SendTextDataToActiveWebsockets(strPtr);
    }
#endif
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
#ifdef USE_WEB_INTERFACE
    //stop websocket after thread
    if (m_websocketServerPtr) {
        m_websocketServerPtr->Stop();
        m_websocketServerPtr.reset();
    }
#endif
}
