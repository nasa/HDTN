/**
 * @file TelemetryServer.h
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 * This TelemetryServer class wraps a ZMQ socket to facilitate
 * communication with the Telemetry module
 * 
 */

#ifndef TELEMETRY_SERVER_H
#define TELEMETRY_SERVER_H 1

#include <zmq.hpp>

#include "telemetry_definitions_export.h"
#include "TelemetryDefinitions.h"

/**
 * TelemetryRequest represents a request for telemetry from the Telemetry module
 */
class TelemetryRequest {
    public:
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TelemetryRequest(bool error);
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TelemetryRequest(bool error, bool more, std::string& message, zmq::message_t& connectionId);

        /**
         * Sends a string response to the provided socket
         */
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS void SendResponse(std::shared_ptr<std::string> strPtr, std::unique_ptr<zmq::socket_t>& socket);
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS void SendResponse(const std::string& resp, std::unique_ptr<zmq::socket_t>& socket);

        /**
         * Sends a success response to the provided socket
         */
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS void SendResponseSuccess(std::unique_ptr<zmq::socket_t>& socket);

        /*
        * Sends an error response to the provided socket
        */
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS void SendResponseError(const std::string& message, std::unique_ptr<zmq::socket_t>& socket);

        /**
         * Gets whether there are more commands associated with this request 
         */
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool More();

        /**
         * Gets whether there was an error processing the request 
         */
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool Error();

        /**
         * Gets the underlying API command
         */
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS std::shared_ptr<ApiCommand_t> Command();

    private:
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS void SendResponse(zmq::message_t& msg, std::unique_ptr<zmq::socket_t>& socket);

        std::shared_ptr<ApiCommand_t> m_cmd;
        zmq::message_t m_connectionId;
        bool m_more;
        bool m_error;
};

/**
 * TelemetryServer wraps a ZMQ socket to facilitate communication with the Telemetry module
 */
class TelemetryServer {
    public:
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TelemetryServer();

        /**
         * Reads a new request from the provided socket 
         */
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TelemetryRequest ReadRequest(std::unique_ptr<zmq::socket_t>& socket);
};

#endif // TELEMETRY_SERVER_H
