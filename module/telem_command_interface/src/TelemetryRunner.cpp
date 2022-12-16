#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/make_unique.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/tokenizer.hpp>

#include "TelemetryRunner.h"
#include "Logger.h"
#include "SignalHandler.h"
#include "Telemetry.h"
#include "TelemetryConnection.h"
#include "TelemetryConnectionPoller.h"
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


bool TelemetryRunner::Run(int argc, const char *const argv[], volatile bool &running)
{
    running = true;

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()("help", "Produce help message.");
    TelemetryRunnerProgramOptions::AppendToDesc(desc);
    boost::program_options::variables_map vm;
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive),
        vm);
    boost::program_options::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return false;
    }
    TelemetryRunnerProgramOptions options;
    if (!options.ParseFromVariableMap(vm)) {
        return false;
    }

    if (!Init(NULL, options)) {
        return false;
    }

    m_runningFromSigHandler = true;
    SignalHandler sigHandler(boost::bind(&TelemetryRunner::MonitorExitKeypressThreadFunc, this));
    sigHandler.Start(false);
    while (running && m_runningFromSigHandler)
    {
        boost::this_thread::sleep(boost::posix_time::millisec(250));
        sigHandler.PollOnce();
    }
    return true;
}

bool TelemetryRunner::Init(zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions &options)
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
        boost::bind(&TelemetryRunner::ThreadFunc, this, inprocContextPtr)); // create and start the worker thread
    return true;
}

void TelemetryRunner::ThreadFunc(zmq::context_t *inprocContextPtr)
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
    catch (...)
    {
        return;
    }

    // Create poller and add each connection
    TelemetryConnectionPoller poller;
    poller.AddConnection(*ingressConnection);
    poller.AddConnection(*egressConnection);
    poller.AddConnection(*storageConnection);

    // Start loop to begin polling
    DeadlineTimer deadlineTimer(THREAD_POLL_INTERVAL_MS);
    while (m_running)
    {
        if (!deadlineTimer.Sleep()) {
            return;
        }

        // Send signals to all hdtn modules
        static const zmq::const_buffer byteSignalBuf(&GUI_REQ_MSG, sizeof(GUI_REQ_MSG));
        ingressConnection->SendMessage(byteSignalBuf);
        egressConnection->SendMessage(byteSignalBuf);
        storageConnection->SendMessage(byteSignalBuf);

        // Wait for telemetry from all modules
        unsigned int receiveEventsMask = 0;
        Metrics::metrics_t metrics;
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
                ProcessIngressTelem(telem, metrics);
            }
            if (poller.HasNewMessage(*egressConnection)) {
                receiveEventsMask |= REC_EGRESS;
                EgressTelemetry_t telem = egressConnection->ReadMessage<EgressTelemetry_t>();
                ProcessEgressTelem(telem, metrics);
            }
            if (poller.HasNewMessage(*storageConnection)) {
                receiveEventsMask |= REC_STORAGE;
                StorageTelemetry_t telem = storageConnection->ReadMessage<StorageTelemetry_t>();
                ProcessStorageTelem(telem, metrics);
            }
        }
        // Handle the newly received metrics
        if (receiveEventsMask == REC_ALL) {
            OnNewMetrics(metrics);
        } else {
            LOG_WARNING(subprocess) << "did not get telemetry from all modules";
        }
    }
    std::cout << "ThreadFunc exiting\n";
}

void TelemetryRunner::ProcessIngressTelem(IngressTelemetry_t &telem, Metrics::metrics_t &metrics)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime startTime = nowTime;
    static boost::posix_time::ptime lastTime = nowTime;
    static double lastTotalBytes = 0;

    // Wait one cycle before performing calculations
    if (nowTime == lastTime) {
        return;
    }

    metrics.ingressCurrentRateMbps = Metrics::CalculateMbpsRate(
        telem.totalDataBytes,
        lastTotalBytes,
        nowTime,
        lastTime);
    metrics.ingressAverageRateMbps = Metrics::CalculateMbpsRate(
        telem.totalDataBytes,
        0,
        nowTime,
        startTime);
    metrics.bundleCountSentToEgress = telem.bundleCountEgress;
    metrics.bundleCountSentToStorage = telem.bundleCountStorage;
    metrics.ingressTotalDataBytes = telem.totalDataBytes;
    metrics.ingressCurrentDataBytes = telem.totalDataBytes - lastTotalBytes;

    lastTime = nowTime;
    lastTotalBytes = telem.totalDataBytes;
}

void TelemetryRunner::ProcessEgressTelem(EgressTelemetry_t &telem, Metrics::metrics_t &metrics)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime lastTime = nowTime;
    static boost::posix_time::ptime startTime = nowTime;
    static double lastTotalBytes = 0;

    // Wait one cycle before performing calculations
    if (nowTime == lastTime) {
        return;
    }

    metrics.egressCurrentRateMbps = Metrics::CalculateMbpsRate(
        telem.totalDataBytes,
        lastTotalBytes,
        nowTime,
        lastTime);
    metrics.egressAverageRateMbps = Metrics::CalculateMbpsRate(
        telem.totalDataBytes,
        0,
        nowTime,
        startTime);
    metrics.egressBundleCount = telem.egressBundleCount;
    metrics.egressMessageCount = telem.egressMessageCount;
    metrics.egressTotalDataBytes = telem.totalDataBytes;
    metrics.egressCurrentDataBytes = telem.totalDataBytes - lastTotalBytes;

    lastTime = nowTime;
    lastTotalBytes = telem.totalDataBytes;
}

void TelemetryRunner::ProcessStorageTelem(StorageTelemetry_t &telem, Metrics::metrics_t &metrics)
{
    metrics.totalBundlesErasedFromStorage = telem.totalBundlesErasedFromStorage;
    metrics.totalBunglesSentFromEgressToStorage = telem.totalBundlesSentToEgressFromStorage;
}

void TelemetryRunner::OnNewMetrics(Metrics::metrics_t metrics)
{
    if (m_websocketServerPtr) {
        m_websocketServerPtr->SendNewBinaryData((const char *)(&metrics), sizeof(metrics));
    }
    if (m_telemetryLoggerPtr) {
        m_telemetryLoggerPtr->LogMetrics(metrics);
    }
}

bool TelemetryRunner::ShouldExit()
{
    if (m_websocketServerPtr) {
        return m_websocketServerPtr->RequestsExit();
    }
    return false;
}

void TelemetryRunner::MonitorExitKeypressThreadFunc()
{
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; // do this first
}

void TelemetryRunner::Stop()
{
    m_running = false;
    if (m_threadPtr) {
        m_threadPtr->join();
        m_threadPtr.reset(); // delete it
    }
}

TelemetryRunner::~TelemetryRunner()
{
    Stop();
}
