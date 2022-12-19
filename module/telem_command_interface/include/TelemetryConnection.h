/**
 * @file TelemetryConnection.h
 *
 * @copyright Copyright Ⓒ 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This TelemetryConnection class implements initializing and interacting with a ZMQ connection for receiving
 * telemetry data from HDTN.
 */

#ifndef TELEMETRY_CONNECTION_H
#define TELEMETRY_CONNECTION_H 1

#include "zmq.hpp"

#include "telem_lib_export.h"

class TelemetryConnection
{
    public:
        TELEM_LIB_EXPORT TelemetryConnection(std::string addr, zmq::context_t* inprocContextPtr);
        TELEM_LIB_EXPORT ~TelemetryConnection();

        /**
         * Sends a new message on the connnection 
         */
        TELEM_LIB_EXPORT bool SendMessage(zmq::const_buffer buffer);

        /**
         * Reads a new message from the connection, if available 
         */
        TELEM_LIB_EXPORT template <typename T> T ReadMessage();

        /**
         * Gets the underlying socket handle 
         */
        TELEM_LIB_EXPORT void* GetSocketHandle();

    private:
        TELEM_LIB_NO_EXPORT TelemetryConnection();
        TELEM_LIB_NO_EXPORT std::string m_addr;
        TELEM_LIB_NO_EXPORT std::unique_ptr<zmq::socket_t> m_requestSocket;
        TELEM_LIB_NO_EXPORT std::unique_ptr<zmq::context_t> m_contextPtr;
};

#endif //TELEMETRY_CONNECTION_H