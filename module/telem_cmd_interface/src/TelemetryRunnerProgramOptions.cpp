/**
 * @file TelemetryRunnerProgramOptions.cpp
 *
 * @copyright Copyright (c) 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 */

#include "TelemetryRunnerProgramOptions.h"
#include "Environment.h"
#include "Logger.h"
#include <boost/filesystem/operations.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::telem;


TelemetryRunnerProgramOptions::TelemetryRunnerProgramOptions() {}

static HdtnDistributedConfig_ptr GetHdtnDistributedConfigPtr(boost::program_options::variables_map& vm) {
    HdtnDistributedConfig_ptr hdtnDistributedConfig;
    if (vm.count("hdtn-distributed-config-file")) {
        const boost::filesystem::path distributedConfigFileName = vm["hdtn-distributed-config-file"].as<boost::filesystem::path>();
        hdtnDistributedConfig = HdtnDistributedConfig::CreateFromJsonFilePath(distributedConfigFileName);
        if (!hdtnDistributedConfig) {
            LOG_ERROR(subprocess) << "error loading HDTN distributed config file: " << distributedConfigFileName;
        }
    }
    return hdtnDistributedConfig;
}

bool TelemetryRunnerProgramOptions::ParseFromVariableMap(boost::program_options::variables_map& vm) {
    m_hdtnDistributedConfigPtr = GetHdtnDistributedConfigPtr(vm); //could be null if not distributed
    return m_websocketServerProgramOptions.ParseFromVariableMap(vm);
}

void TelemetryRunnerProgramOptions::AppendToDesc(boost::program_options::options_description& desc) {
    WebsocketServer::ProgramOptions::AppendToDesc(desc);
}
