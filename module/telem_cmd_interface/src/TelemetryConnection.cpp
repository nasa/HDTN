/**
 * @file TelemetryConnection.cpp
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
 */

#include <boost/make_unique.hpp>

#include "TelemetryConnection.h"
#include "TelemetryDefinitions.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::telem;

static void CustomCleanupStdString(void* data, void* hint) {
    delete static_cast<std::string*>(hint);
}

static void CustomCleanupZmqMessage(void *data, void *hint) {
    delete static_cast<zmq::message_t*>(hint);
}

TelemetryConnection::TelemetryConnection(
    const std::string& addr,
    zmq::context_t* contextPtr,
    zmq::socket_type socketType,
    bool bind
) {
    try {
        if (!contextPtr) {
            m_context = boost::make_unique<zmq::context_t>();
            m_socket = boost::make_unique<zmq::socket_t>(*m_context, socketType);
        } else {
            m_socket = boost::make_unique<zmq::socket_t>(*contextPtr, socketType);
        }
        m_socket->set(zmq::sockopt::linger, 0);
        if (bind) {
            m_socket->bind(addr);
        } else {
            m_socket->connect(addr);
        }
        m_addr = addr;
    }
    catch (const zmq::error_t & ex) {
        LOG_ERROR(subprocess) << "cannot connect zmq socket: " << ex.what();
        if (m_socket) {
            m_socket->close();
        }
        throw;
    }
}

void TelemetryConnection::SendRequests()
{
    boost::mutex::scoped_lock lock(m_apiCallsMutex);
    while (!m_apiCallsQueue.empty()) {
        const bool moreFlag = (m_apiCallsQueue.size() > 1);
        zmq_api_msg_plus_connection_id_pair_t& p = m_apiCallsQueue.front();
        // First send the connection ID, then the request body
        SendZmqMessage(std::move(p.second), true);
        SendZmqMessage(std::move(p.first), moreFlag);
        m_apiCallsQueue.pop();
    }
}

bool TelemetryConnection::EnqueueApiPayload(std::string&& payload, zmq::message_t&& connectionID)
{
    std::string* apiCmdStr = new std::string(std::move(payload));
    std::string& apiCmdStrRef = *apiCmdStr;

    zmq::message_t* connectionIdMsg = new zmq::message_t(std::move(connectionID));
    zmq::message_t& connectionIdMsgRef = *connectionIdMsg;

    boost::mutex::scoped_lock lock(m_apiCallsMutex);
    m_apiCallsQueue.emplace(std::piecewise_construct,
        std::forward_as_tuple(&apiCmdStrRef[0], apiCmdStrRef.size(), CustomCleanupStdString, apiCmdStr),
        std::forward_as_tuple(connectionIdMsgRef.data(), connectionIdMsgRef.size(), CustomCleanupZmqMessage, connectionIdMsg));
    return true;
}

bool TelemetryConnection::SendZmqConstBufferMessage(const zmq::const_buffer& buffer, bool more) {
    const zmq::send_flags additionalFlags = (more) ? zmq::send_flags::sndmore : zmq::send_flags::none;
    try {
        if (!m_socket->send(buffer, zmq::send_flags::dontwait | additionalFlags)) {
            LOG_ERROR(subprocess) << "error sending zmq signal to socket " << m_addr;
            return false;
        }
    }
    catch (zmq::error_t &) {
        LOG_INFO(subprocess) << "request already sent to socket " << m_addr;
        return false;
    }
    return true;
}

bool TelemetryConnection::SendZmqMessage(zmq::message_t&& zmqMessage, bool more) {
    const zmq::send_flags additionalFlags = (more) ? zmq::send_flags::sndmore : zmq::send_flags::none;
    try {
        if (!m_socket->send(std::move(zmqMessage), zmq::send_flags::dontwait | additionalFlags)) {
            LOG_ERROR(subprocess) << "error sending zmq message to socket " << m_addr;
            return false;
        }
    }
    catch (zmq::error_t&) {
        LOG_INFO(subprocess) << "request already sent to socket " << m_addr;
        return false;
    }
    return true;
}

zmq::message_t TelemetryConnection::ReadMessage()
{
    zmq::message_t msg;
    const zmq::recv_result_t res = m_socket->recv(msg, zmq::recv_flags::dontwait);
    if (!res) {
        LOG_ERROR(subprocess) << "cannot read telemetry message from address " << m_addr;
    }
    return msg;
}

void* TelemetryConnection::GetSocketHandle()
{
    if (!m_socket) {
        return NULL;
    }
    return m_socket->handle();
}

TelemetryConnection::~TelemetryConnection()
{
    m_socket.reset();
    if (m_context) {
        m_context.reset();
    }
}
