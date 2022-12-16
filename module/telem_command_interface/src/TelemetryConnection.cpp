#include <boost/make_unique.hpp>

#include "TelemetryConnection.h"
#include "Telemetry.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::gui;

/**
 * Declare all possible types for the ReadMessage template 
 */
template IngressTelemetry_t TelemetryConnection::ReadMessage();
template EgressTelemetry_t TelemetryConnection::ReadMessage();
template StorageTelemetry_t TelemetryConnection::ReadMessage();
template uint8_t TelemetryConnection::ReadMessage();


TelemetryConnection::TelemetryConnection(std::string addr, zmq::context_t* inprocContextPtr) {
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
    } catch (const zmq::error_t & ex) {
        LOG_ERROR(subprocess) << "cannot connect zmq socket: " << ex.what();
        if (m_requestSocket) {
            m_requestSocket->close();
        }
        throw;
    }
}

bool TelemetryConnection::SendMessage(zmq::const_buffer buffer) {
    try {
        if (!m_requestSocket->send(buffer, zmq::send_flags::dontwait)) {
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

template <typename T> T TelemetryConnection::ReadMessage()
{
    T telem;
    const zmq::recv_buffer_result_t res = m_requestSocket->recv(zmq::mutable_buffer(&telem, sizeof(telem)), zmq::recv_flags::dontwait);
    if (!res) {
        LOG_ERROR(subprocess) << "cannot read telemetry message from address " << m_addr;
    }
    else if ((res->truncated()) || (res->size != sizeof(telem))) {
        LOG_ERROR(subprocess) << "telemetry message mismatch from address " << m_addr << ": untruncated = " << res->untruncated_size
            << " truncated = " << res->size << " expected = " << sizeof(telem);
    }
    return telem;
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
