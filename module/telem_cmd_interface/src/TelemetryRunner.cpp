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
#include "Metrics.h"
#include "Environment.h"
#include "DeadlineTimer.h"
#include "WebsocketServer.h"


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
        bool Init(zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions& options);
        bool ShouldExit();
        void Stop();

    private:
        void ThreadFunc(zmq::context_t * inprocContextPtr);
        void OnNewMetrics(Metrics::metrics_t metrics);

        volatile bool m_running;
        std::unique_ptr<boost::thread> m_threadPtr;
        std::unique_ptr<WebsocketServer> m_websocketServerPtr;
        std::unique_ptr<TelemetryLogger> m_telemetryLoggerPtr;
};

/**
 * TelemetryRunner proxies 
 */
TelemetryRunner::TelemetryRunner()
    : m_pimpl(boost::make_unique<TelemetryRunner::Impl>())
{}

bool TelemetryRunner::Init(zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions &options)
{
    return m_pimpl->Init(inprocContextPtr, options);
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

TelemetryRunner::Impl::Impl()
    : m_running(false)
    {}

bool TelemetryRunner::Impl::Init(zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions &options)
{
#ifdef USE_WEB_INTERFACE
    m_websocketServerPtr = boost::make_unique<WebsocketServer>();
    m_websocketServerPtr->Init(options.m_guiDocumentRoot, options.m_guiPortNumber);
#endif

#ifdef DO_STATS_LOGGING
    m_telemetryLoggerPtr = boost::make_unique<TelemetryLogger>();
#endif

    m_running = true;
    m_threadPtr = boost::make_unique<boost::thread>(
        boost::bind(&TelemetryRunner::Impl::ThreadFunc, this, inprocContextPtr)); // create and start the worker thread
    return true;
}

void TelemetryRunner::Impl::ThreadFunc(zmq::context_t *inprocContextPtr)
{
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
            ingressConnection = boost::make_unique<TelemetryConnection>("tcp://localhost:10303", nullptr);
            egressConnection = boost::make_unique<TelemetryConnection>("tcp://localhost:10302", nullptr);
            storageConnection = boost::make_unique<TelemetryConnection>("tcp://localhost:10301", nullptr);
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
    Metrics metrics;
    DeadlineTimer deadlineTimer(THREAD_POLL_INTERVAL_MS);
    while (m_running)
    {
        if (!deadlineTimer.Sleep()) {
            return;
        }

        // Send signals to all hdtn modules
        static const zmq::const_buffer byteSignalBuf(&TELEM_REQ_MSG, sizeof(TELEM_REQ_MSG));
        ingressConnection->SendMessage(byteSignalBuf);
        egressConnection->SendMessage(byteSignalBuf);
        storageConnection->SendMessage(byteSignalBuf);

        // Wait for telemetry from all modules
        metrics.Clear();
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
                IngressTelemetry_t telem = ingressConnection->ReadMessage<IngressTelemetry_t>();
                metrics.ProcessIngressTelem(telem);
            }
            if (poller.HasNewMessage(*egressConnection)) {
                receiveEventsMask |= REC_EGRESS;
                EgressTelemetry_t telem = egressConnection->ReadMessage<EgressTelemetry_t>();
                metrics.ProcessEgressTelem(telem);
            }
            if (poller.HasNewMessage(*storageConnection)) {
                receiveEventsMask |= REC_STORAGE;
                StorageTelemetry_t telem = storageConnection->ReadMessage<StorageTelemetry_t>();
                metrics.ProcessStorageTelem(telem);
            }
        }
        // Handle the newly received metrics
        if (receiveEventsMask == REC_ALL) {
            OnNewMetrics(metrics.Get());
        } else {
            LOG_WARNING(subprocess) << "did not get telemetry from all modules";
        }
    }
    LOG_DEBUG(subprocess) << "ThreadFunc exiting";
}

void TelemetryRunner::Impl::OnNewMetrics(Metrics::metrics_t metrics)
{
    if (m_websocketServerPtr) {
        m_websocketServerPtr->SendNewBinaryData((const char *)(&metrics), sizeof(metrics));
    }
    if (m_telemetryLoggerPtr) {
        m_telemetryLoggerPtr->LogMetrics(metrics);
    }
}

bool TelemetryRunner::Impl::ShouldExit()
{
    if (m_websocketServerPtr) {
        return m_websocketServerPtr->RequestsExit();
    }
    return false;
}

void TelemetryRunner::Impl::Stop()
{
    m_running = false;
    if (!m_threadPtr) {
        return;
    }
    try {
        m_threadPtr->join();
    } catch (std::exception& e) {
        LOG_WARNING(subprocess) << e.what();
    }
    m_threadPtr.reset(); // delete it
}
