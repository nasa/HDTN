#include <iostream>
#include <vector>

#include "store.hpp"
#include "StorageRunner.h"
#include "message.hpp"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>


void StorageRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}


StorageRunner::StorageRunner() {}
StorageRunner::~StorageRunner() {}


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
#ifdef USE_BRIAN_STORAGE
                ("storage-config-json-file", boost::program_options::value<std::string>()->default_value("storageConfig.json"), "Listen on this TCP or UDP port.")
#else
                ("storage-path", boost::program_options::value<std::string>()->default_value("/home/hdtn/hdtn.store"), "Listen on this TCP or UDP port.")
#endif // USE_BRIAN_STORAGE
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return 1;
            }

#ifdef USE_BRIAN_STORAGE
            storePath = vm["storage-config-json-file"].as<std::string>();
#else
            storePath = vm["storage-path"].as<std::string>();
#endif
        }
        catch (boost::bad_any_cast & e) {
            std::cout << "invalid data error: " << e.what() << "\n\n";
            std::cout << desc << "\n";
            return 1;
        }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
        catch (...) {
            std::cerr << "Exception of unknown type!\n";
            return 1;
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
        hdtn::storage store;
        std::cout << "[store] Initializing storage manager ..." << std::endl;


        if (!store.init(config)) {
            return -1;
        }

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "egress up and running" << std::endl;
        while (running && m_runningFromSigHandler) {            
            store.update();
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
        store.Stop();
        m_totalBundlesErasedFromStorage = store.m_totalBundlesErasedFromStorage;
        m_totalBundlesSentToEgressFromStorage = store.m_totalBundlesSentToEgressFromStorage;
    }
    std::cout << "StorageRunner: exited cleanly\n";
    return true;
}
