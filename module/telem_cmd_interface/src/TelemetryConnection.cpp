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


TelemetryConnection::TelemetryConnection(const std::string& addr, zmq::context_t* inprocContextPtr) {
    try {
        if (inprocContextPtr) {
            m_requestSocket = boost::make_unique<zmq::socket_t>(*inprocContextPtr, zmq::socket_type::pair);
        } else {
            m_contextPtr = boost::make_unique<zmq::context_t>();
            m_requestSocket = boost::make_unique<zmq::socket_t>(*m_contextPtr, zmq::socket_type::req);
        }
        m_requestSocket->set(zmq::sockopt::linger, 0);
        m_requestSocket->connect(addr);
        m_addr = addr;
    }
    catch (const zmq::error_t & ex) {
        LOG_ERROR(subprocess) << "cannot connect zmq socket: " << ex.what();
        if (m_requestSocket) {
            m_requestSocket->close();
        }
        throw;
    }
}

bool TelemetryConnection::SendZmqConstBufferMessage(const zmq::const_buffer& buffer, bool more) {
    const zmq::send_flags additionalFlags = (more) ? zmq::send_flags::sndmore : zmq::send_flags::none;
    try {
        if (!m_requestSocket->send(buffer, zmq::send_flags::dontwait | additionalFlags)) {
            LOG_ERROR(subprocess) << "error sending zmq signal";
            return false;
        }
    }
    catch (zmq::error_t &) {
        LOG_INFO(subprocess) << "request already sent";
        return false;
    }
    return true;
}

bool TelemetryConnection::SendZmqMessage(zmq::message_t&& zmqMessage, bool more) {
    const zmq::send_flags additionalFlags = (more) ? zmq::send_flags::sndmore : zmq::send_flags::none;
    try {
        if (!m_requestSocket->send(std::move(zmqMessage), zmq::send_flags::dontwait | additionalFlags)) {
            LOG_ERROR(subprocess) << "error sending zmq message";
            return false;
        }
    }
    catch (zmq::error_t&) {
        LOG_INFO(subprocess) << "request already sent";
        return false;
    }
    return true;
}

zmq::message_t TelemetryConnection::ReadMessage()
{
    zmq::message_t msg;
    const zmq::recv_result_t res = m_requestSocket->recv(msg, zmq::recv_flags::dontwait);
    if (!res) {
        LOG_ERROR(subprocess) << "cannot read telemetry message from address " << m_addr;
    }
    return msg;
}

void* TelemetryConnection::GetSocketHandle()
{
    if (!m_requestSocket) {
        return NULL;
    }
    return m_requestSocket->handle();
}

TelemetryConnection::~TelemetryConnection()
{
    m_requestSocket.reset();
    m_contextPtr.reset();
}
