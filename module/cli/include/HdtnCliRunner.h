/**
 * @file HdtnCliRunner.h
 * @author Ethan Schweinsberg <ethan.e.schweinsberg@nasa.gov>
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
 *
 * This HdtnCliRunner class is used for launching HDTN CLI utility.
 * The HdtnCliRunner connects to a running HDTN application over a ZMQ socket and issues API commands
 * using the REQ/REP pattern.
 */

#ifndef _HDTN_CLI_RUNNER_H
#define _HDTN_CLI_RUNNER_H 1

#include <boost/program_options.hpp>
#include <zmq.hpp>

#include "HdtnConfig.h"
#include "hdtn_cli_lib_export.h"



class HdtnCliRunner {
public:
    HDTN_CLI_LIB_EXPORT HdtnCliRunner();
    HDTN_CLI_LIB_EXPORT ~HdtnCliRunner();

    /**
     * Run the HDTN CLI utility.
     * @param argc The number of arguments.
     * @param argv The arguments.
     * 
     * @return True if the CLI utility was successfully run.
     */
    HDTN_CLI_LIB_EXPORT bool Run(int argc, const char* const argv[]);

    /**
     * Parse the hostname and port from the command line.
     * @param argc The number of arguments.
     * @param argv The arguments.
     * @param hostname The hostname retrieved from the command line.
     * @param port The port retrieved from the command line.
     * 
     * @return True if the hostname and port were successfully parsed.
     */
    HDTN_CLI_LIB_EXPORT bool ParseHostnameAndPort(int argc, const char* const argv[], std::string& hostname, std::string& port);

    /**
     * Connect to HDTN.
     * @param hostname The hostname to connect to.
     * @param port The port to connect to.
     * 
     * @return True if the connection was successful.
    */
    HDTN_CLI_LIB_EXPORT bool ConnectToHdtn(std::string& hostname, std::string& port);

    /**
     * Get the HDTN config file from HDTN
     */
    HDTN_CLI_LIB_EXPORT HdtnConfig_ptr GetHdtnConfig();

    /**
     * Send a request to HDTN.
     * @param msg The message to send.
     * 
     * @return The response from HDTN.
    */
    HDTN_CLI_LIB_EXPORT std::string SendRequestToHdtn(const std::string& msg);

    /**
     * Parse the rest of the command line options.
     * @param argc The number of arguments.
     * @param argv The arguments.
     * @param config The HDTN config file to use.
     * @param vm The variables map to store the parsed options.
     * 
     * @return True if the options were successfully parsed.
    */
    HDTN_CLI_LIB_EXPORT bool ParseCliOptions(int argc,
        const char* const argv[],
        HdtnConfig_ptr config,
        boost::program_options::variables_map& vm);

    /**
     * Execute the options parsed from the command line.
     * @param vm The variables map containing the parsed options.
    */
    HDTN_CLI_LIB_EXPORT bool ExecuteCliOptions(boost::program_options::variables_map& vm);

private:
    zmq::context_t m_context;
    std::unique_ptr<zmq::socket_t> m_socket;
    zmq::pollitem_t m_pollitems[1];
};

#endif
