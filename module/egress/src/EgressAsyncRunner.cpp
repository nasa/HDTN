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
#include "reg.hpp"

void EgressAsyncRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
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
        OutductsConfig_ptr outductsConfig;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("outducts-config-file", boost::program_options::value<std::string>()->default_value("outducts.json"), "Outducts Configuration File.")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return false;
            }

            const std::string configFileName = vm["outducts-config-file"].as<std::string>();

            outductsConfig = OutductsConfig::CreateFromJsonFile(configFileName);
            if (!outductsConfig) {
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
        hdtn::HdtnRegsvr regsvr;
        regsvr.Init(HDTN_REG_SERVER_PATH, "egress", 10100, "PULL");
        regsvr.Reg();
        if (hdtn::HdtnEntries_ptr res = regsvr.Query()) {
            const hdtn::HdtnEntryList_t & entryList = res->m_hdtnEntryList;
            for (hdtn::HdtnEntryList_t::const_iterator it = entryList.cbegin(); it != entryList.cend(); ++it) {
                const hdtn::HdtnEntry & entry = *it;
                std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
                hdtn::Logger::getInstance()->logInfo("egress", std::string(entry.address) + ":" + std::to_string(entry.port) + ":" + entry.mode);
            }
        }
        else {
            std::cerr << "error: null registration query" << std::endl;
            hdtn::Logger::getInstance()->logError("egress", "Error: null registration query");
            return 1;
        }
        hdtn::HegrManagerAsync egress;
        egress.Init(*outductsConfig);

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
        m_bundleCount = egress.m_bundleCount;
        m_bundleData = egress.m_bundleData;
        m_messageCount = egress.m_messageCount;
    }
    std::cout << "EgressAsyncRunner: exited cleanly\n";
    hdtn::Logger::getInstance()->logNotification("egress", "EgressAsyncRunner: Exited cleanly");
    return true;
}
