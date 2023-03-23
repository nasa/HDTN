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

#include <queue>

#include <boost/thread.hpp>
#include "zmq.hpp"

#include "telem_lib_export.h"
#include "TelemetryDefinitions.h"

enum class ApiSource_t {
    webgui,
    socket
};

class TelemetryConnection
{
    public:
        TELEM_LIB_EXPORT TelemetryConnection(const std::string& addr, zmq::context_t* inprocContextPtr, bool bind = false);
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


        /**
         * Sends a new request for telemetry. Handles sending queued API calls.
         * @param alwaysRequest whether to always request data, even if there are no API calls queued
         */
        TELEM_LIB_EXPORT void SendRequest(bool alwaysRequest = true);

        /**
         * Enqueues a new API payload to be sent on the next request 
         */
        TELEM_LIB_EXPORT bool EnqueueApiPayload(std::string&& payload, ApiSource_t src);

        bool m_apiSocketAwaitingResponse;
    private:
        TelemetryConnection() = delete;
        std::string m_addr;
        std::unique_ptr<zmq::socket_t> m_requestSocket;
        std::unique_ptr<zmq::context_t> m_contextPtr;
        typedef std::pair<zmq::message_t, ApiSource_t> zmq_api_msg_plus_source_pair_t;
        std::queue<zmq_api_msg_plus_source_pair_t> m_apiCallsQueue;
        boost::mutex m_apiCallsMutex;
};

#endif //TELEMETRY_CONNECTION_H