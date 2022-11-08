/**
 * @file StorageRunner.cpp
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

#include <vector>
#include "StorageRunner.h"
#include "message.hpp"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::storage;

void StorageRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
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

        HdtnConfig_ptr hdtnConfig;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("hdtn-config-file", boost::program_options::value<std::string>()->default_value("hdtn.json"), "HDTN Configuration File.");

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
            return false;
        }
        catch (std::exception& e) {
            LOG_ERROR(subprocess) << "error: " << e.what();
            return false;
        }
        catch (...) {
            LOG_ERROR(subprocess) << "Exception of unknown type!";
            return false;
        }
        //double last = 0.0;
        //timeval tv;
        //gettimeofday(&tv, NULL);
        //last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
        //hdtn::storageConfig config;
        //config.regsvr = HDTN_REG_SERVER_PATH;
        //config.local = HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH;
        //config.releaseWorker = HDTN_BOUND_SCHEDULER_PUBSUB_PATH;
        //telem(HDTN_STORAGE_TELEM_PATH), worker(HDTN_STORAGE_WORKER_PATH), releaseWorker(HDTN_BOUND_SCHEDULER_PUBSUB_PATH) {}
        //config.storePath = storePath;
        m_storagePtr = boost::make_unique<ZmqStorageInterface>();
        LOG_INFO(subprocess) << "Initializing storage manager ...";
        if (!m_storagePtr->Init(*hdtnConfig)) {
            return false;
        }

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        LOG_INFO(subprocess) << "storage up and running";

        while (running && m_runningFromSigHandler) {            
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
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
        LOG_INFO(subprocess) << "StorageRunner: exiting cleanly..";
//        store.Stop();
//        m_totalBundlesErasedFromStorage = store.m_totalBundlesErasedFromStorage;
//        m_totalBundlesSentToEgressFromStorage = store.m_totalBundlesSentToEgressFromStorage;
        m_storagePtr->Stop();
        m_totalBundlesErasedFromStorage = m_storagePtr->GetCurrentNumberOfBundlesDeletedFromStorage();
        m_totalBundlesSentToEgressFromStorage = m_storagePtr->m_totalBundlesSentToEgressFromStorage;
    }
    LOG_INFO(subprocess) << "StorageRunner: exited cleanly";
    return true;
}
