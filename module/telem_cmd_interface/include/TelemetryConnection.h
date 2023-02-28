/**
 * @file TelemetryConnection.h
 *
 * @copyright Copyright © 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 * This TelemetryConnection class implements initializing and handling message i/o on a ZMQ connection,
 * specifically for receiving telemetry data from HDTN.
 */

#ifndef TELEMETRY_CONNECTION_H
#define TELEMETRY_CONNECTION_H 1

#include "zmq.hpp"

#include "telem_lib_export.h"

class TelemetryConnection
{
    public:
        TELEM_LIB_EXPORT TelemetryConnection(const std::string& addr, zmq::context_t* inprocContextPtr);
        TELEM_LIB_EXPORT ~TelemetryConnection();

        /**
         * Sends a new message on the connnection
         * @param buffer the zmq::const_buffer to send
         */
        TELEM_LIB_EXPORT bool SendZmqConstBufferMessage(const zmq::const_buffer& buffer, bool more); //note: SendMessage is a reserved define in Windows

        TELEM_LIB_EXPORT bool SendZmqMessage(zmq::message_t&& zmqMessage, bool more);

        /**
         * Reads a new message from the connection, if available 
         */
        TELEM_LIB_EXPORT zmq::message_t ReadMessage();

        /**
         * Gets the underlying socket handle 
         */
        TELEM_LIB_EXPORT void* GetSocketHandle();

    private:
        TelemetryConnection() = delete;
        std::string m_addr;
        std::unique_ptr<zmq::socket_t> m_requestSocket;
        std::unique_ptr<zmq::context_t> m_contextPtr;
};

#endif //TELEMETRY_CONNECTION_H