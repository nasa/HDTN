/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include "ingress.h"
#include "store.hpp"
#include "EgressAsync.h"
#include "HdtnOneProcessRunner.h"
#include "SignalHandler.h"

#include <fstream>
#include <iostream>
#include "Logger.h"
#include "message.hpp"
#include "reg.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>
#include <boost/make_unique.hpp>


void HdtnOneProcessRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    hdtn::Logger::getInstance()->logNotification("ingress", "Keyboard Interrupt");
    m_runningFromSigHandler = false; //do this first
}


HdtnOneProcessRunner::HdtnOneProcessRunner() {}
HdtnOneProcessRunner::~HdtnOneProcessRunner() {}

bool HdtnOneProcessRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&HdtnOneProcessRunner::MonitorExitKeypressThreadFunction, this));
        bool isCutThroughOnlyTest = false;
        HdtnConfig_ptr hdtnConfig;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("cut-through-only-test", "Always send to egress.  Assume all links always available.")
                ("hdtn-config-file", boost::program_options::value<std::string>()->default_value("hdtn.json"), "HDTN Configuration File.")
    	        ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return false;
            }

            if (vm.count("cut-through-only-test")) {
                isCutThroughOnlyTest = true;
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
            hdtn::Logger::getInstance()->logError("ingress", "Invalid Data Error: " + std::string(e.what()));
            std::cout << desc << "\n";
            return false;
        }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            hdtn::Logger::getInstance()->logError("ingress", "Error: " + std::string(e.what()));
            return false;
        }
        catch (...) {
            std::cerr << "Exception of unknown type!\n";
            hdtn::Logger::getInstance()->logError("ingress", "Exception of unknown type!");
            return false;
        }

        //The io_threads argument specifies the size of the 0MQ thread pool to handle I/O operations.
        //If your application is using only the inproc transport for messaging you may set this to zero, otherwise set it to at least one.     
        std::unique_ptr<zmq::context_t> hdtnOneProcessZmqInprocContextPtr = boost::make_unique<zmq::context_t>(0);// 0 Threads

        std::cout << "starting EgressAsync.." << std::endl;
        hdtn::Logger::getInstance()->logNotification("egress", "Starting EgressAsync");
        /*
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
        }*/
        hdtn::HegrManagerAsync egress;
        egress.Init(*hdtnConfig, hdtnOneProcessZmqInprocContextPtr.get());

        printf("Announcing presence of egress ...\n");
        hdtn::Logger::getInstance()->logNotification("egress", "Egress Present");
        {
            hdtn::HdtnRegsvr regsvr;
            const std::string connect_regServerPath(
                std::string("tcp://") +
                hdtnConfig->m_zmqRegistrationServerAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnConfig->m_zmqRegistrationServerPortPath));
            regsvr.Init(connect_regServerPath, "egress", hdtnConfig->m_zmqBoundIngressToConnectingEgressPortPath, "PULL");
            regsvr.Reg();
        }
        


        std::cout << "starting ingress.." << std::endl;
        hdtn::Logger::getInstance()->logNotification("ingress", "Starting Ingress");
        hdtn::Ingress ingress;
        ingress.Init(*hdtnConfig, isCutThroughOnlyTest, hdtnOneProcessZmqInprocContextPtr.get());

        // finish registration stuff -ingress will find out what egress services have
        // registered
        {
            hdtn::HdtnRegsvr regsvr;
            const std::string connect_regServerPath(
                std::string("tcp://") +
                hdtnConfig->m_zmqRegistrationServerAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnConfig->m_zmqRegistrationServerPortPath));
            regsvr.Init(connect_regServerPath, "ingress", hdtnConfig->m_zmqBoundIngressToConnectingEgressPortPath, "PUSH");
            regsvr.Reg();
        
            if (hdtn::HdtnEntries_ptr res = regsvr.Query()) {
                const hdtn::HdtnEntryList_t & entryList = res->m_hdtnEntryList;
                for (hdtn::HdtnEntryList_t::const_iterator it = entryList.cbegin(); it != entryList.cend(); ++it) {
                    const hdtn::HdtnEntry & entry = *it;
                    std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
                    hdtn::Logger::getInstance()->logNotification("ingress", "Entry: " + entry.address + ":" + 
                        std::to_string(entry.port) + ":" + entry.mode);
                }
            }
            else {
                std::cerr << "error: null registration query" << std::endl;
                hdtn::Logger::getInstance()->logError("ingress", "Error: null registration query");
                return false;
            }
        }

        hdtn::storage storage;
        std::cout << "[store] Initializing storage manager ..." << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "[store] Initializing storage manager ...");
        if (!storage.Init(*hdtnConfig, hdtnOneProcessZmqInprocContextPtr.get())) {
            return false;
        }
        

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "ingress, egress, and storage up and running" << std::endl;
        while (running && m_runningFromSigHandler) {
            storage.update(); //sleep 250
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        std::ofstream output;
//        output.open("ingress-" + currentDate);

        std::ostringstream oss;
        oss << "Elapsed, Bundle Count (M),Rate (Mbps),Bundles/sec, Bundle Data "
            "(MB)\n";
        double rate = 8 * ((ingress.m_bundleData / (double)(1024 * 1024)) / ingress.m_elapsed);
        oss << ingress.m_elapsed << "," << ingress.m_bundleCount / 1000000.0f << "," << rate << ","
            << ingress.m_bundleCount / ingress.m_elapsed << ", " << ingress.m_bundleData / (double)(1024 * 1024) << "\n";

        std::cout << oss.str();
//        output << oss.str();
//        output.close();
        hdtn::Logger::getInstance()->logInfo("ingress", "Elapsed: " + std::to_string(ingress.m_elapsed) + 
                                                        ", Bundle Count (M): " + std::to_string(ingress.m_bundleCount / 1000000.0f) +
                                                        ", Rate (Mbps): " + std::to_string(rate) +
                                                        ", Bundles/sec: " + std::to_string(ingress.m_bundleCount / ingress.m_elapsed) +
                                                        ", Bundle Data (MP): " + std::to_string(ingress.m_bundleData / (double)(1024 * 1024)));

        boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
        std::cout << "IngressAsyncRunner currentTime  " << timeLocal << std::endl << std::flush;

        std::cout << "IngressAsyncRunner: exiting cleanly..\n";
        ingress.Stop();
        m_ingressBundleCountStorage = ingress.m_bundleCountStorage;
        m_ingressBundleCountEgress = ingress.m_bundleCountEgress;
        m_ingressBundleCount = ingress.m_bundleCount;
        m_ingressBundleData = ingress.m_bundleData;

        std::cout << "StorageRunner: exiting cleanly..\n";
        storage.Stop();
        m_totalBundlesErasedFromStorage = storage.m_totalBundlesErasedFromStorage;
        m_totalBundlesSentToEgressFromStorage = storage.m_totalBundlesSentToEgressFromStorage;

        std::cout << "EgressAsyncRunner: exiting cleanly..\n";
        egress.Stop();
        m_egressBundleCount = egress.m_bundleCount;
        m_egressBundleData = egress.m_bundleData;
        m_egressMessageCount = egress.m_messageCount;
    }
    std::cout << "HDTN one process: exited cleanly\n";
    return true;
}
