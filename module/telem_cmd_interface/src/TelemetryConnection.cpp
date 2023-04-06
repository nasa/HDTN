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

TelemetryConnection::TelemetryConnection(const std::string& addr, zmq::context_t* inprocContextPtr, bool bind) :
    m_apiSocketAwaitingResponse(false)
{
    try {
        if (inprocContextPtr) {
            m_requestSocket = boost::make_unique<zmq::socket_t>(*inprocContextPtr, zmq::socket_type::pair);
        } else {
            m_contextPtr = boost::make_unique<zmq::context_t>();
            zmq::socket_type socketType = bind ? zmq::socket_type::rep : zmq::socket_type::req;
            m_requestSocket = boost::make_unique<zmq::socket_t>(*m_contextPtr, socketType);
        }
        m_requestSocket->set(zmq::sockopt::linger, 0);
        if (bind) {
            m_requestSocket->bind(addr);
        } else {
            m_requestSocket->connect(addr);
        }
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

void TelemetryConnection::SendRequest(bool alwaysRequest)
{
    static const zmq::const_buffer byteSignalBuf(&TELEM_REQ_MSG, sizeof(TELEM_REQ_MSG));
    static const zmq::const_buffer byteSignalBufPlusApi(&TELEM_REQ_MSG_PLUS_API_CALLS, sizeof(TELEM_REQ_MSG_PLUS_API_CALLS));        
    {
        boost::mutex::scoped_lock lock(m_apiCallsMutex);
        if (alwaysRequest && m_apiCallsQueue.empty()) {
            SendZmqConstBufferMessage(byteSignalBuf, false);
        }
        else if (!m_apiCallsQueue.empty()) {
            if (!SendZmqConstBufferMessage(byteSignalBufPlusApi, true)) {
                LOG_WARNING(subprocess) << "delaying sending API call until next interval";
                return;
            }
        }
        while (!m_apiCallsQueue.empty()) {
            const bool moreFlag = (m_apiCallsQueue.size() > 1);
            zmq_api_msg_plus_source_pair_t& p = m_apiCallsQueue.front();
            SendZmqMessage(std::move(p.first), moreFlag);
            const ApiSource_t& src = p.second;
            if (src == ApiSource_t::socket) {
                m_apiSocketAwaitingResponse = true;
            }
            m_apiCallsQueue.pop();
        }
    }
}

bool TelemetryConnection::EnqueueApiPayload(std::string&& payload, ApiSource_t src)
{
    std::string* apiCmdStr = new std::string(std::move(payload));
    std::string& strRef = *apiCmdStr;
    boost::mutex::scoped_lock lock(m_apiCallsMutex);
    m_apiCallsQueue.emplace(std::piecewise_construct,
        std::forward_as_tuple(&strRef[0], strRef.size(), CustomCleanupStdString, apiCmdStr),
        std::forward_as_tuple(src));
    return true;
}

bool TelemetryConnection::SendZmqConstBufferMessage(const zmq::const_buffer& buffer, bool more) {
    const zmq::send_flags additionalFlags = (more) ? zmq::send_flags::sndmore : zmq::send_flags::none;
    try {
        if (!m_requestSocket->send(buffer, zmq::send_flags::dontwait | additionalFlags)) {
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
        if (!m_requestSocket->send(std::move(zmqMessage), zmq::send_flags::dontwait | additionalFlags)) {
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
