/**
 * @file EgressAsyncRunner.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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

#include "EgressAsync.h"
#include "Logger.h"
#include "EgressAsyncRunner.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::egress;

void EgressAsyncRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}


EgressAsyncRunner::EgressAsyncRunner() {}
EgressAsyncRunner::~EgressAsyncRunner() {}


bool EgressAsyncRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&EgressAsyncRunner::MonitorExitKeypressThreadFunction, this));
        HdtnConfig_ptr hdtnConfig;
        HdtnDistributedConfig_ptr hdtnDistributedConfig;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("hdtn-config-file", boost::program_options::value<boost::filesystem::path>()->default_value("hdtn.json"), "HDTN Configuration File.")
                ("hdtn-distributed-config-file", boost::program_options::value<boost::filesystem::path>()->default_value("hdtn_distributed.json"), "HDTN Distributed Mode Configuration File.")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                LOG_INFO(subprocess) << desc;
                return false;
            }

            const boost::filesystem::path configFileName = vm["hdtn-config-file"].as<boost::filesystem::path>();
            hdtnConfig = HdtnConfig::CreateFromJsonFilePath(configFileName);
            if (!hdtnConfig) {
                LOG_ERROR(subprocess) << "error loading HDTN config file: " << configFileName;
                return false;
            }

            const boost::filesystem::path distributedConfigFileName = vm["hdtn-distributed-config-file"].as<boost::filesystem::path>();
            hdtnDistributedConfig = HdtnDistributedConfig::CreateFromJsonFilePath(distributedConfigFileName);
            if (!hdtnDistributedConfig) {
                LOG_ERROR(subprocess) << "error loading HDTN distributed config file: " << distributedConfigFileName;
                return false;
            }
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


        LOG_INFO(subprocess) << "starting EgressAsync..";
        
        hdtn::Egress egress;
        if (!egress.Init(*hdtnConfig, *hdtnDistributedConfig)) {
            return false;
        }

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        LOG_INFO(subprocess) << "egress up and running";
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        LOG_INFO(subprocess) << "EgressAsyncRunner: exiting cleanly..";
        egress.Stop();
        m_bundleCount = egress.m_telemetry.egressBundleCount;
        m_bundleData = static_cast<uint64_t>(egress.m_telemetry.totalDataBytes);
        m_messageCount = egress.m_telemetry.egressMessageCount;
    }
    LOG_INFO(subprocess) << "EgressAsyncRunner: exited cleanly";
    return true;
}
