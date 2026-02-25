/**
 * @file TelemetryRunnerProgramOptions.h
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
 * @section DESCRIPTION
 * This TelemetryRunnerProgramOptions class stores the program options for the TelemetryRunner
 */

#ifndef TELEMETRY_RUNNER_PROGRAM_OPTIONS_H
#define TELEMETRY_RUNNER_PROGRAM_OPTIONS_H 1

#include "HdtnDistributedConfig.h"
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include "WebsocketServer.h"
#include "telem_lib_export.h"


class TelemetryRunnerProgramOptions
{
    public:
        TELEM_LIB_EXPORT TelemetryRunnerProgramOptions();

        /**
         * Appends program options to an existing options_description object
         */
        TELEM_LIB_EXPORT static void AppendToDesc(boost::program_options::options_description& desc);

        /**
         * Parses a variable map and stores the result 
         */
        TELEM_LIB_EXPORT bool ParseFromVariableMap(boost::program_options::variables_map& vm);

        
public:
        /**
         * Program options
         */
        HdtnDistributedConfig_ptr m_hdtnDistributedConfigPtr;
        WebsocketServer::ProgramOptions m_websocketServerProgramOptions;
};

#endif // TELEMETRY_RUNNER_PROGRAM_OPTIONS_H
