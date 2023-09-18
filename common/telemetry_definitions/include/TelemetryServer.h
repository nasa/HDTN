/**
 * @file TelemetryServer.h
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 * This TelemetryServer class serves as a wrapper around a ZMQ socket, facilitating
 * communication with the Telemetry module for handling API commands
 * 
 */

#ifndef TELEMETRY_SERVER_H
#define TELEMETRY_SERVER_H 1

#include <zmq.hpp>

#include "telemetry_definitions_export.h"
#include "TelemetryDefinitions.h"

class TelemetryRequest {
    public:
        TELEMETRY_DEFINITIONS_EXPORT TelemetryRequest();
        TELEMETRY_DEFINITIONS_EXPORT void SendResponse(std::shared_ptr<std::string> strPtr, std::unique_ptr<zmq::socket_t>& socket);
        TELEMETRY_DEFINITIONS_EXPORT void SendResponse(const std::string& resp, std::unique_ptr<zmq::socket_t>& socket);
        TELEMETRY_DEFINITIONS_EXPORT void SendResponseSuccess(std::unique_ptr<zmq::socket_t>& socket);
        TELEMETRY_DEFINITIONS_EXPORT void SendResponseError(const std::string& message, std::unique_ptr<zmq::socket_t>& socket);

        std::shared_ptr<ApiCommand_t> cmd;
        zmq::message_t m_connectionId;
        bool m_more;
        bool error;

    private:
        TELEMETRY_DEFINITIONS_EXPORT void SendResponse(zmq::message_t& msg, std::unique_ptr<zmq::socket_t>& socket);
};

class TelemetryServer {
    public:
        TELEMETRY_DEFINITIONS_EXPORT TelemetryServer();
        TELEMETRY_DEFINITIONS_EXPORT TelemetryRequest ReadRequest(std::unique_ptr<zmq::socket_t>& socket);
};

#endif // TELEMETRY_SERVER_H