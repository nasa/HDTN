/**
 * @file SchedulerRunner.cpp
 * @author Nadia Kortas <nadia.kortas@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "scheduler.h"
#include "Logger.h"
#include "SchedulerRunner.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

static const boost::filesystem::path DEFAULT_FILE = "contactPlan.json";
static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::scheduler;

void SchedulerRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}


SchedulerRunner::SchedulerRunner() : m_runningFromSigHandler(false) {}
SchedulerRunner::~SchedulerRunner() {}


bool SchedulerRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&SchedulerRunner::MonitorExitKeypressThreadFunction, this));
        HdtnConfig_ptr hdtnConfig;
        HdtnDistributedConfig_ptr hdtnDistributedConfig;
        bool usingUnixTimestamp;
        boost::filesystem::path contactPlanFilePath;

        namespace opt = boost::program_options;

        opt::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("use-unix-timestamp", "Use unix timestamp in contact plan.")
                ("hdtn-config-file", opt::value<boost::filesystem::path>()->default_value("hdtn.json"), "HDTN Configuration File.")
                ("hdtn-distributed-config-file", boost::program_options::value<boost::filesystem::path>()->default_value("hdtn_distributed.json"), "HDTN Distributed Mode Configuration File.")
                ("contact-plan-file", opt::value<boost::filesystem::path>()->default_value(DEFAULT_FILE), "Contact Plan file that scheduler relies on for link availability.");

            opt::variables_map vm;
            opt::store(boost::program_options::parse_command_line(argc, argv, desc, opt::command_line_style::unix_style | opt::command_line_style::case_insensitive), vm);
            opt::notify(vm);

            if (vm.count("help")) {
                LOG_INFO(subprocess) << desc;
                return false;
            }

            const boost::filesystem::path configFileName = vm["hdtn-config-file"].as<boost::filesystem::path>();

            hdtnConfig = HdtnConfig::CreateFromJsonFilePath(configFileName);
            if (!hdtnConfig) {
                LOG_ERROR(subprocess) << "error loading config file: " << configFileName;
                return false;
            }

            const boost::filesystem::path distributedConfigFileName = vm["hdtn-distributed-config-file"].as<boost::filesystem::path>();
            hdtnDistributedConfig = HdtnDistributedConfig::CreateFromJsonFilePath(distributedConfigFileName);
            if (!hdtnDistributedConfig) {
                LOG_ERROR(subprocess) << "error loading HDTN distributed config file: " << distributedConfigFileName;
                return false;
            }

            usingUnixTimestamp = (vm.count("use-unix-timestamp") != 0);

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


        LOG_INFO(subprocess) << "Starting scheduler..";
        
        Scheduler scheduler;
        if (!scheduler.Init(*hdtnConfig, *hdtnDistributedConfig, contactPlanFilePath, usingUnixTimestamp)) {
            return false;
        }

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        LOG_INFO(subprocess) << "Scheduler up and running";
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        LOG_INFO(subprocess) << "SchedulerRunner: exiting cleanly..";
        scheduler.Stop();
    }
    LOG_INFO(subprocess) << "SchedulerRunner: exited cleanly";
    return true;
}
