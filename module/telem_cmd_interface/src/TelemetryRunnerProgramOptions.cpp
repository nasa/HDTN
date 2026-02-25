/**
 * @file TelemetryRunnerProgramOptions.cpp
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
