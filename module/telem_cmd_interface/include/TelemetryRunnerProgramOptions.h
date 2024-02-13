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
#include "BeastWebsocketServer.h" //for determining if BEAST_WEBSOCKET_SERVER_SUPPORT_SSL when adding options
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
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
#ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
        struct SslPaths {
            boost::filesystem::path m_certificatePemFile; //not preferred
            boost::filesystem::path m_certificateChainPemFile; //preferred
            boost::filesystem::path m_privateKeyPemFile;
            boost::filesystem::path m_diffieHellmanParametersPemFile;
            bool m_valid = false;
        };
        SslPaths m_sslPaths;
private:
        void GetSslPathsAndValidate(boost::program_options::variables_map& vm, SslPaths& sslPaths);
public:
#endif
        /**
         * Program options
         */
        boost::filesystem::path m_guiDocumentRoot;
        std::string m_guiPortNumber;
        HdtnDistributedConfig_ptr m_hdtnDistributedConfigPtr;



    private:
        boost::filesystem::path GetDocumentRootAndValidate(boost::program_options::variables_map& vm);
        std::string GetPortNumberAsString(boost::program_options::variables_map& vm);
        HdtnDistributedConfig_ptr GetHdtnDistributedConfigPtr(boost::program_options::variables_map& vm);
};

#endif // TELEMETRY_RUNNER_PROGRAM_OPTIONS_H
