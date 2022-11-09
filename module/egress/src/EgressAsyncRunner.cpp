/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
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

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("hdtn-config-file", boost::program_options::value<std::string>()->default_value("hdtn.json"), "HDTN Configuration File.")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                LOG_INFO(subprocess) << desc;
                return false;
            }

            const std::string configFileName = vm["hdtn-config-file"].as<std::string>();

            hdtnConfig = HdtnConfig::CreateFromJsonFile(configFileName);
            if (!hdtnConfig) {
                LOG_ERROR(subprocess) << "error loading config file: " << configFileName;
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
        
	hdtn::HegrManagerAsync egress;
        egress.Init(*hdtnConfig);

        LOG_DEBUG(subprocess) << "Announcing presence of egress...";
        
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
        m_bundleData = static_cast<uint64_t>(egress.m_telemetry.egressBundleData);
        m_messageCount = egress.m_telemetry.egressMessageCount;
    }
    LOG_INFO(subprocess) << "EgressAsyncRunner: exited cleanly";
    return true;
}
