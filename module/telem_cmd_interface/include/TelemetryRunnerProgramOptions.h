/**
 * @file TelemetryRunnerProgramOptions.h
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
