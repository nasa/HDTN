/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include <iostream>
#include "EgressAsync.h"
#include "EgressAsyncRunner.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

void EgressAsyncRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    hdtn::Logger::getInstance()->logNotification("egress", "Keyboard Interrupt.. exiting");
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
                std::cout << desc << "\n";
                return false;
            }

            const std::string configFileName = vm["hdtn-config-file"].as<std::string>();

            hdtnConfig = HdtnConfig::CreateFromJsonFile(configFileName);
            if (!hdtnConfig) {
                std::cerr << "error loading config file: " << configFileName << std::endl;
                return false;
            }
        }
        catch (boost::bad_any_cast & e) {
            std::cout << "invalid data error: " << e.what() << "\n\n";
            hdtn::Logger::getInstance()->logError("egress", "Invalid data error: " + std::string(e.what()));
            std::cout << desc << "\n";
            return false;
        }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            hdtn::Logger::getInstance()->logError("egress", "Error: " + std::string(e.what()));
            return false;
        }
        catch (...) {
            std::cerr << "Exception of unknown type!\n";
            hdtn::Logger::getInstance()->logError("egress", "Exception of unknown type!");
            return false;
        }


        std::cout << "starting EgressAsync.." << std::endl;
        hdtn::Logger::getInstance()->logNotification("egress", "Starting EgressAsync");
        
	hdtn::HegrManagerAsync egress;
        egress.Init(*hdtnConfig);

        printf("Announcing presence of egress ...\n");
        hdtn::Logger::getInstance()->logNotification("egress", "Egress Present");
        
        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "egress up and running" << std::endl;
        hdtn::Logger::getInstance()->logNotification("egress", "Egress up and running.");
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        std::cout << "EgressAsyncRunner: exiting cleanly..\n";
        egress.Stop();
        m_bundleCount = egress.m_telemetry.egressBundleCount;
        m_bundleData = egress.m_telemetry.egressBundleData;
        m_messageCount = egress.m_telemetry.egressMessageCount;
    }
    std::cout << "EgressAsyncRunner: exited cleanly\n";
    hdtn::Logger::getInstance()->logNotification("egress", "EgressAsyncRunner: Exited cleanly");
    return true;
}
