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
#include "scheduler.h"
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

HdtnOneProcessRunner::HdtnOneProcessRunner() {}
HdtnOneProcessRunner::~HdtnOneProcessRunner() {}

bool HdtnOneProcessRunner::Run(int argc, const char *const argv[], volatile bool &running, bool useSignalHandler)
{
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&HdtnOneProcessRunner::MonitorExitKeypressThreadFunction, this));

        HdtnConfig_ptr hdtnConfig;
        bool usingUnixTimestamp;
        bool useMgr;
        boost::filesystem::path contactPlanFilePath;


#ifdef RUN_TELEMETRY
        TelemetryRunnerProgramOptions telemetryRunnerOptions;
#endif

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("hdtn-config-file", boost::program_options::value<boost::filesystem::path>()->default_value("hdtn.json"), "HDTN Configuration File.")
                ("contact-plan-file", boost::program_options::value<boost::filesystem::path>()->default_value(DEFAULT_CONTACT_FILE), "Contact Plan file that scheduler relies on for link availability.")
                ("use-unix-timestamp", "Use unix timestamp in contact plan.")
                ("use-mgr", "Use Multigraph Routing Algorithm")
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

            usingUnixTimestamp = (vm.count("use-unix-timestamp") != 0);

            useMgr = (vm.count("use-mgr") != 0);

            contactPlanFilePath = vm["contact-plan-file"].as<boost::filesystem::path>();
            if (contactPlanFilePath.empty()) {
                LOG_INFO(subprocess) << desc;
                return false;
            }

            if (!boost::filesystem::exists(contactPlanFilePath)) { //first see if the user specified an already valid path name not dependent on HDTN's source root
                contactPlanFilePath = Scheduler::GetFullyQualifiedFilename(contactPlanFilePath);
                if (!boost::filesystem::exists(contactPlanFilePath)) {
                    LOG_ERROR(subprocess) << "ContactPlan File not found: " << contactPlanFilePath;
                    return false;
                }
            }

            LOG_INFO(subprocess) << "ContactPlan file: " << contactPlanFilePath;
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

        LOG_INFO(subprocess) << "starting Scheduler..";
        Scheduler scheduler;
        if (!scheduler.Init(*hdtnConfig, contactPlanFilePath, usingUnixTimestamp, hdtnOneProcessZmqInprocContextPtr.get())) {
            return false;
        }

        LOG_INFO(subprocess) << "starting Router..";
        Router router;
        if (!router.Init(*hdtnConfig, contactPlanFilePath, usingUnixTimestamp, useMgr, hdtnOneProcessZmqInprocContextPtr.get())) {
            return false;
        }

        LOG_INFO(subprocess) << "starting Egress..";
        //No need to create Egress, Ingress, and Storage on heap with unique_ptr to prevent stack overflows because they use the pimpl pattern
        hdtn::Egress egress;
        if (!egress.Init(*hdtnConfig, hdtnOneProcessZmqInprocContextPtr.get())) {
            return false;
        }

        LOG_INFO(subprocess) << "starting Ingress..";
        hdtn::Ingress ingress;
        if (!ingress.Init(*hdtnConfig, hdtnOneProcessZmqInprocContextPtr.get())) {
            return false;
        }

        ZmqStorageInterface storage;
        LOG_INFO(subprocess) << "starting Storage..";
        if (!storage.Init(*hdtnConfig, hdtnOneProcessZmqInprocContextPtr.get())) {
            return false;
        }

#ifdef RUN_TELEMETRY
        LOG_INFO(subprocess) << "Initializing telemetry runner...";
        TelemetryRunner telemetryRunner;
        if (!telemetryRunner.Init(hdtnOneProcessZmqInprocContextPtr.get(), telemetryRunnerOptions))
        {
            return false;
        }
#endif

        if (useSignalHandler) {
            sigHandler.Start(false);
        }

#ifdef RUN_TELEMETRY
        while (running && m_runningFromSigHandler && !telemetryRunner.ShouldExit()) {
#else
        while (running && m_runningFromSigHandler) {
#endif // RUN_TELEMETRY
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        LOG_INFO(subprocess) << "Elapsed, Bundle Count (M), Rate (Mbps), Bundles/sec, Bundle Data (MB) ";
        //Possibly out of Date
        double rate = 8 * ((ingress.m_bundleData / (double)(1024 * 1024)) / ingress.m_elapsed);
        LOG_INFO(subprocess) << ingress.m_elapsed << "," << ingress.m_bundleCount / 1000000.0f << "," << rate << ","
            << ingress.m_bundleCount / ingress.m_elapsed << ", " << ingress.m_bundleData / (double)(1024 * 1024);

#ifdef RUN_TELEMETRY
        LOG_INFO(subprocess) << "TelemetryRunner: exiting cleanly...";
        telemetryRunner.Stop();
#endif

        boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
        LOG_INFO(subprocess) << "IngressAsyncRunner currentTime  " << timeLocal;

        LOG_INFO(subprocess) << "IngressAsyncRunner: exiting cleanly..";
        ingress.Stop();
        m_ingressBundleCountStorage = ingress.m_bundleCountStorage;
        m_ingressBundleCountEgress = ingress.m_bundleCountEgress;
        m_ingressBundleCount = ingress.m_bundleCount;
        m_ingressBundleData = ingress.m_bundleData;

        LOG_INFO(subprocess) << "StorageRunner: exiting cleanly..";
        storage.Stop();
        m_totalBundlesErasedFromStorage = storage.GetCurrentNumberOfBundlesDeletedFromStorage();
        m_totalBundlesSentToEgressFromStorage = storage.m_totalBundlesSentToEgressFromStorageReadFromDisk;

        LOG_INFO(subprocess) << "EgressAsyncRunner: exiting cleanly..";
        egress.Stop();
        m_egressBundleCount = egress.m_telemetry.egressBundleCount;
        m_egressBundleData = static_cast<uint64_t>(egress.m_telemetry.totalDataBytes);
        m_egressMessageCount = egress.m_telemetry.egressMessageCount;
    }
    LOG_INFO(subprocess) << "HDTN one process: exited cleanly";
    return true;
}
