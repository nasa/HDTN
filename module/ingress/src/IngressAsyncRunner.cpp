#include "ingress.h"
#include "IngressAsyncRunner.h"
#include "SignalHandler.h"

#include <fstream>
#include <iostream>

#include "Logger.h"
#include "message.hpp"
#include "reg.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

#define BP_INGRESS_TELEM_FREQ (0.10)
#define INGRESS_PORT (4556)

void IngressAsyncRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    hdtn::Logger::getInstance()->logNotification("ingress", "Keyboard Interrupt");
    m_runningFromSigHandler = false; //do this first
}


IngressAsyncRunner::IngressAsyncRunner() {}
IngressAsyncRunner::~IngressAsyncRunner() {}


bool IngressAsyncRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&IngressAsyncRunner::MonitorExitKeypressThreadFunction, this));
        bool useStcp = false;
        bool alwaysSendToStorage = false;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                //("port", boost::program_options::value<boost::uint16_t>()->default_value(4557), "Listen on this TCP or UDP port.")
                ("use-stcp", "Use STCP instead of TCPCL.")
                ("always-send-to-storage", "Don't send straight to egress (for testing).")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return false;
            }


            if (vm.count("use-stcp")) {
                useStcp = true;
            }

            if (vm.count("always-send-to-storage")) {
                alwaysSendToStorage = true;
            }

            //port = vm["port"].as<boost::uint16_t>();
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


        std::cout << "starting ingress.." << std::endl;
        hdtn::Logger::getInstance()->logNotification("ingress", "Starting Ingress");
        hdtn::BpIngress ingress;
        ingress.Init(BP_INGRESS_TYPE_UDP);

        // finish registration stuff -ingress will find out what egress services have
        // registered
        hdtn::HdtnRegsvr regsvr;
        regsvr.Init(HDTN_REG_SERVER_PATH, "ingress", 10100, "PUSH");
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


        printf("Announcing presence of ingress engine ...\n");

        ingress.Netstart(INGRESS_PORT, !useStcp, useStcp, alwaysSendToStorage);

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "ingress up and running" << std::endl;
        hdtn::Logger::getInstance()->logNotification("ingress", "Ingress up and running");
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        std::string currentDate = hdtn::Datetime();
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

        std::cout << "IngressAsyncRunner: exiting cleanly..\n";
        ingress.Stop();
        m_bundleCountStorage = ingress.m_bundleCountStorage;
        m_bundleCountEgress = ingress.m_bundleCountEgress;
        m_bundleCount = ingress.m_bundleCount;
        m_bundleData = ingress.m_bundleData;
    }
    std::cout << "IngressAsyncRunner: exited cleanly\n";
    hdtn::Logger::getInstance()->logNotification("ingress", "IngressAsyncRunner: exited cleanly");
    return true;
}
