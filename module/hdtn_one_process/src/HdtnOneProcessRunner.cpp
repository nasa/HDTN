/**
 * @file HdtnOneProcessRunner.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "ingress.h"
#include "ZmqStorageInterface.h"
#include "EgressAsync.h"
#include "router.h"
#include "HdtnOneProcessRunner.h"
#include "SignalHandler.h"
#include "Environment.h"

#include <fstream>
#include <iostream>
#include "Logger.h"
#include "message.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>
#include <boost/make_unique.hpp>
#ifdef RUN_TELEMETRY
#include "TelemetryRunner.h"
#include "TelemetryRunnerProgramOptions.h"
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;
static const boost::filesystem::path DEFAULT_CONTACT_FILE = "contactPlan.json";

void HdtnOneProcessRunner::MonitorExitKeypressThreadFunction()
{
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}

HdtnOneProcessRunner::HdtnOneProcessRunner() :
    //ingress
    m_ingressBundleCountStorage(0),
    m_ingressBundleCountEgress(0),
    m_ingressBundleCount(0),
    m_ingressBundleData(0),

    //egress
    m_egressTotalBundlesGivenToOutducts(0),
    m_egressTotalBundleBytesGivenToOutducts(0),

    //storage
    m_totalBundlesErasedFromStorage(0),
    m_totalBundlesSentToEgressFromStorage(0),

    m_runningFromSigHandler(false)
{}
HdtnOneProcessRunner::~HdtnOneProcessRunner() {}

bool HdtnOneProcessRunner::Run(int argc, const char *const argv[], volatile bool &running, bool useSignalHandler)
{
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&HdtnOneProcessRunner::MonitorExitKeypressThreadFunction, this));

        HdtnConfig_ptr hdtnConfig;
        boost::filesystem::path bpSecConfigFilePath;

        HdtnDistributedConfig unusedHdtnDistributedConfig;
        
        bool usingUnixTimestamp;
        bool useMgr;
        boost::filesystem::path contactPlanFilePath;
        std::string leiderImpl;

#ifdef RUN_TELEMETRY
        TelemetryRunnerProgramOptions telemetryRunnerOptions;
#endif

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("hdtn-config-file", boost::program_options::value<boost::filesystem::path>()->default_value("hdtn.json"), "HDTN Configuration File.")
                ("bpsec-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "BpSec Configuration File.")
                ("contact-plan-file", boost::program_options::value<boost::filesystem::path>()->default_value(DEFAULT_CONTACT_FILE), "Contact Plan file that router relies on for link availability.")
                ("use-unix-timestamp", "Use unix timestamp in contact plan.")
                ("use-mgr", "Use Multigraph Routing Algorithm")
                ("leider", boost::program_options::value<std::string>()->default_value("redundant"), "Which LEIDer implementation to use")
    	        ;
#ifdef RUN_TELEMETRY
            TelemetryRunnerProgramOptions::AppendToDesc(desc);
#endif

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                LOG_INFO(subprocess) << desc;
                return false;
            }
#ifdef RUN_TELEMETRY
            telemetryRunnerOptions.ParseFromVariableMap(vm);
#endif
            const boost::filesystem::path configFileName = vm["hdtn-config-file"].as<boost::filesystem::path>();

            hdtnConfig = HdtnConfig::CreateFromJsonFilePath(configFileName);
            if (!hdtnConfig) {
                LOG_ERROR(subprocess) << "error loading config file: " << configFileName;
                return false;
            }

            bpSecConfigFilePath = vm["bpsec-config-file"].as<boost::filesystem::path>();

            usingUnixTimestamp = (vm.count("use-unix-timestamp") != 0);

            useMgr = (vm.count("use-mgr") != 0);

            contactPlanFilePath = vm["contact-plan-file"].as<boost::filesystem::path>();
            if (contactPlanFilePath.empty()) {
                LOG_INFO(subprocess) << desc;
                return false;
            }

            if (!boost::filesystem::exists(contactPlanFilePath)) { //first see if the user specified an already valid path name not dependent on HDTN's source root
                contactPlanFilePath = Router::GetFullyQualifiedFilename(contactPlanFilePath);
                if (!boost::filesystem::exists(contactPlanFilePath)) {
                    LOG_ERROR(subprocess) << "ContactPlan File not found: " << contactPlanFilePath;
                    return false;
                }
            }

            LOG_INFO(subprocess) << "ContactPlan file: " << contactPlanFilePath;

            leiderImpl = vm["leider"].as<std::string>();
        }
        catch (boost::bad_any_cast & e) {
            LOG_ERROR(subprocess) << "invalid data error: " << e.what();
            LOG_ERROR(subprocess) << desc;
            return false;
        }
        catch (std::exception& e) {
            LOG_ERROR(subprocess) << e.what();
            return false;
        }
        catch (...) {
            LOG_ERROR(subprocess) << "Exception of unknown type!";
            return false;
        }

        //The io_threads argument specifies the size of the 0MQ thread pool to handle I/O operations.
        //If your application is using only the inproc transport for messaging you may set this to zero, otherwise set it to at least one.
        std::unique_ptr<zmq::context_t> hdtnOneProcessZmqInprocContextPtr = boost::make_unique<zmq::context_t>(0);// 0 Threads

        LOG_INFO(subprocess) << "starting Router..";
        std::unique_ptr<Router> routerPtr = boost::make_unique<Router>();
        if (!routerPtr->Init(*hdtnConfig, unusedHdtnDistributedConfig, contactPlanFilePath, usingUnixTimestamp, useMgr, hdtnOneProcessZmqInprocContextPtr.get())) {
            return false;
        }

        LOG_INFO(subprocess) << "starting Egress..";
        //No need to create Egress, Ingress, and Storage on heap with unique_ptr to prevent stack overflows because they use the pimpl pattern
        //However, the unique_ptr reset() function is useful for isolating destructor hangs on exit
        std::unique_ptr<hdtn::Egress> egressPtr = boost::make_unique<hdtn::Egress>();
        if (!egressPtr->Init(*hdtnConfig, unusedHdtnDistributedConfig, hdtnOneProcessZmqInprocContextPtr.get())) {
            return false;
        }

        LOG_INFO(subprocess) << "starting Ingress..";
        std::unique_ptr<hdtn::Ingress> ingressPtr = boost::make_unique<hdtn::Ingress>();
        if (!ingressPtr->Init(*hdtnConfig, bpSecConfigFilePath,
            unusedHdtnDistributedConfig,
            hdtnOneProcessZmqInprocContextPtr.get(),
            leiderImpl))
        {
            return false;
        }

        LOG_INFO(subprocess) << "starting Storage..";
        std::unique_ptr<ZmqStorageInterface> storagePtr = boost::make_unique<ZmqStorageInterface>();
        if (!storagePtr->Init(*hdtnConfig, unusedHdtnDistributedConfig,
            hdtnOneProcessZmqInprocContextPtr.get()))
        {
            return false;
        }

#ifdef RUN_TELEMETRY
        LOG_INFO(subprocess) << "Starting telemetry runner...";
        std::unique_ptr<TelemetryRunner> telemetryRunnerPtr = boost::make_unique<TelemetryRunner>();
        if (!telemetryRunnerPtr->Init(*hdtnConfig, hdtnOneProcessZmqInprocContextPtr.get(), telemetryRunnerOptions)) {
            return false;
        }
#endif

        if (useSignalHandler) {
            sigHandler.Start(false);
        }

#ifdef RUN_TELEMETRY
        while (running && m_runningFromSigHandler) {
#else
        while (running && m_runningFromSigHandler) {
#endif // RUN_TELEMETRY
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }


#ifdef RUN_TELEMETRY
        LOG_INFO(subprocess) << "Telemetry: stopping..";
        telemetryRunnerPtr->Stop();
        LOG_INFO(subprocess) << "Telemetry: deleting..";
        telemetryRunnerPtr.reset();
#endif


        LOG_INFO(subprocess) << "Router: stopping..";
        routerPtr->Stop();
        LOG_INFO(subprocess) << "Router: deleting..";
        routerPtr.reset();

        boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
        
	LOG_INFO(subprocess) << "Ingress currentTime  " << timeLocal;
        LOG_INFO(subprocess) << "Ingress: stopping..";
        ingressPtr->Stop();
        m_ingressBundleCountStorage = ingressPtr->m_bundleCountStorage;
        m_ingressBundleCountEgress = ingressPtr->m_bundleCountEgress;
        m_ingressBundleCount = (ingressPtr->m_bundleCountEgress + ingressPtr->m_bundleCountStorage);
        m_ingressBundleData = (ingressPtr->m_bundleByteCountEgress + ingressPtr->m_bundleByteCountStorage);
	LOG_INFO(subprocess) << "Ingress Bundle Count (M), Bundle Data (MB)";
        LOG_INFO(subprocess) << m_ingressBundleCount << "," << (m_ingressBundleData / (1024.0 * 1024.0));
        LOG_INFO(subprocess) << "Ingress: deleting..";
        ingressPtr.reset();

        LOG_INFO(subprocess) << "Storage: stopping..";
        storagePtr->Stop();
        m_totalBundlesErasedFromStorage = storagePtr->GetCurrentNumberOfBundlesDeletedFromStorage();
        m_totalBundlesSentToEgressFromStorage = storagePtr->m_telemRef.m_totalBundlesSentToEgressFromStorageReadFromDisk;
        LOG_INFO(subprocess) << "Storage: deleting..";
        storagePtr.reset();

        LOG_INFO(subprocess) << "Egress: stopping..";
        egressPtr->Stop();
        m_egressTotalBundlesGivenToOutducts = egressPtr->m_allOutductTelemRef.m_totalBundlesGivenToOutducts;
        m_egressTotalBundleBytesGivenToOutducts = static_cast<uint64_t>(egressPtr->m_allOutductTelemRef.m_totalBundleBytesGivenToOutducts);
        LOG_INFO(subprocess) << "Egress: deleting..";
        egressPtr.reset();

        LOG_INFO(subprocess) << "Inproc zmq context: deleting..";
        hdtnOneProcessZmqInprocContextPtr.reset();

        LOG_INFO(subprocess) << "All modules deleted successfully from HdtnOneProcess.";
    }
    LOG_INFO(subprocess) << "HDTN one process: exited cleanly";
    return true;
}
