/**
 * @file IngressAsyncRunner.cpp
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

#include "ingress.h"
#include "IngressAsyncRunner.h"
#include "SignalHandler.h"

#include <fstream>
#include <iostream>
#include "Logger.h"
#include "message.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::ingress;


void IngressAsyncRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}


IngressAsyncRunner::IngressAsyncRunner() : 
    m_bundleCountStorage(0),
    m_bundleCountEgress(0),
    m_bundleCount(0),
    m_bundleData(0),
    m_runningFromSigHandler(false) {}
IngressAsyncRunner::~IngressAsyncRunner() {}


bool IngressAsyncRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&IngressAsyncRunner::MonitorExitKeypressThreadFunction, this));
        HdtnConfig_ptr hdtnConfig;
        boost::filesystem::path bpSecConfigFilePath;
        HdtnDistributedConfig_ptr hdtnDistributedConfig;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("hdtn-config-file", boost::program_options::value<boost::filesystem::path>()->default_value("hdtn.json"), "HDTN Configuration File.")
                ("bpsec-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "BpSec Configuration File.")
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
                LOG_ERROR(subprocess) << "error loading config file: " << configFileName;
                return false;
            }

            bpSecConfigFilePath = vm["bpsec-config-file"].as<boost::filesystem::path>();

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

        LOG_INFO(subprocess) << "starting ingress..";
        hdtn::Ingress ingress;
        if (!ingress.Init(*hdtnConfig, bpSecConfigFilePath, *hdtnDistributedConfig)) {
            return false;
        }

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        LOG_INFO(subprocess) << "ingress up and running";
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
        LOG_INFO(subprocess) << "IngressAsyncRunner currentTime  " << timeLocal;

        LOG_INFO(subprocess) << "IngressAsyncRunner: exiting cleanly..";
        ingress.Stop();
        m_bundleCountStorage = ingress.m_bundleCountStorage;
        m_bundleCountEgress = ingress.m_bundleCountEgress;
        m_bundleCount = (ingress.m_bundleCountEgress + ingress.m_bundleCountStorage);
        m_bundleData = (ingress.m_bundleByteCountEgress + ingress.m_bundleByteCountStorage);
        LOG_INFO(subprocess) << "Bundle Count (M), Bundle Data (MB)";
        LOG_INFO(subprocess) << m_bundleCount << "," << (m_bundleData / (1024.0 * 1024.0));
        
    }
    LOG_INFO(subprocess) << "IngressAsyncRunner: exited cleanly";
    return true;
}
