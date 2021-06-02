/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include <iostream>
#include <vector>
#include "store.hpp"
#include "StorageRunner.h"
#include "message.hpp"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>


void StorageRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}


StorageRunner::StorageRunner() {}
StorageRunner::~StorageRunner() {}


std::size_t StorageRunner::GetCurrentNumberOfBundlesDeletedFromStorage() {
    if (!m_storagePtr) {
        return 0;
    }
    return m_storagePtr->GetCurrentNumberOfBundlesDeletedFromStorage();
}


bool StorageRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&StorageRunner::MonitorExitKeypressThreadFunction, this));

        std::string storePath;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("storage-config-json-file", boost::program_options::value<std::string>()->default_value("storageConfig.json"), "Listen on this TCP or UDP port.");

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return false;
            }

            storePath = vm["storage-config-json-file"].as<std::string>();
        }
        catch (boost::bad_any_cast & e) {
            std::cout << "invalid data error: " << e.what() << "\n\n";
            hdtn::Logger::getInstance()->logError("storage", "Invalid data error: " + std::string(e.what()));
            std::cout << desc << "\n";
            return false;
        }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return false;
        }
        catch (...) {
            std::cerr << "Exception of unknown type!\n";
            return false;
        }
        //double last = 0.0;
        //timeval tv;
        //gettimeofday(&tv, NULL);
        //last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
        hdtn::storageConfig config;
        config.regsvr = HDTN_REG_SERVER_PATH;
        config.local = HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH;
        config.releaseWorker = HDTN_BOUND_SCHEDULER_PUBSUB_PATH;
        config.storePath = storePath;
        m_storagePtr = boost::make_unique<hdtn::storage>();
        std::cout << "[store] Initializing storage manager ..." << std::endl;

        if (!m_storagePtr->init(config)) {
            return false;
        }

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "storage up and running" << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "Storage up and running");

        while (running && m_runningFromSigHandler) {            
            m_storagePtr->update();
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
            //gettimeofday(&tv, NULL);
            //double curr = (tv.tv_sec + (tv.tv_usec / 1000000.0));
            /*if (curr - last > 1) {
                last = curr;
                hdtn::StorageStats *stats = store.stats();
                uint64_t cbytes = stats->inBytes - _last_bytes;
                uint64_t ccount = stats->inMsg - _last_count;
                _last_bytes = stats->inBytes;
                _last_count = stats->inMsg;
                std::cout<< "Bytes read:"<< stats->worker.flow.disk_rbytes << " Bytes written: "<< stats->worker.flow.disk_wbytes <<std::endl;
                std::cout<<  "Mbps released: "<< stats->worker.flow.read_rate << "in " << stats->worker.flow.read_ts << " sec" <<std::endl;
                printf("[store] Received: %lu msg / %0.2f MB\n", ccount, cbytes / (1024.0 * 1024.0));
            }*/
        }
        std::cout << "StorageRunner: exiting cleanly..\n";
//        store.Stop();
//        m_totalBundlesErasedFromStorage = store.m_totalBundlesErasedFromStorage;
//        m_totalBundlesSentToEgressFromStorage = store.m_totalBundlesSentToEgressFromStorage;
        m_storagePtr->Stop();
        m_totalBundlesErasedFromStorage = m_storagePtr->m_totalBundlesErasedFromStorage;
        m_totalBundlesSentToEgressFromStorage = m_storagePtr->m_totalBundlesSentToEgressFromStorage;
    }
    std::cout << "StorageRunner: exited cleanly\n";
    hdtn::Logger::getInstance()->logNotification("storage", "StorageRunner: exited cleanly");
    return true;
}
