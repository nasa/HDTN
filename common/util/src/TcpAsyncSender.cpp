#include <string>
#include <iostream>
#include "TcpAsyncSender.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>

TcpAsyncSenderElement::TcpAsyncSenderElement() : m_onSuccessfulSendCallbackByIoServiceThreadPtr(NULL) {}

void TcpAsyncSenderElement::DoCallback(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if(m_onSuccessfulSendCallbackByIoServiceThreadPtr && (*m_onSuccessfulSendCallbackByIoServiceThreadPtr)) {
        (*m_onSuccessfulSendCallbackByIoServiceThreadPtr)(error, bytes_transferred);
    }
}


TcpAsyncSender::TcpAsyncSender(boost::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr, boost::asio::io_service & ioServiceRef) :
    m_ioServiceRef(ioServiceRef),
    m_tcpSocketPtr(tcpSocketPtr),
    m_writeInProgress(false)
{

}

TcpAsyncSender::~TcpAsyncSender() {

}

void TcpAsyncSender::AsyncSend_NotThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    m_queueTcpAsyncSenderElements.push(std::unique_ptr<TcpAsyncSenderElement>(senderElementNeedingDeleted));
    if (!m_writeInProgress) {
        m_writeInProgress = true;
        boost::asio::async_write(*m_tcpSocketPtr, senderElementNeedingDeleted->m_constBufferVec,
            boost::bind(&TcpAsyncSender::HandleTcpSend, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}

void TcpAsyncSender::AsyncSend_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    boost::asio::post(m_ioServiceRef, boost::bind(&TcpAsyncSender::AsyncSend_NotThreadSafe, this, senderElementNeedingDeleted));
}


void TcpAsyncSender::HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred) {
    m_queueTcpAsyncSenderElements.front()->DoCallback(error, bytes_transferred);
    m_queueTcpAsyncSenderElements.pop();
    if (error) {
        std::cerr << "error in TcpAsyncSender::HandleTcpSend: " << error.message() << std::endl;
    }
    else if (m_queueTcpAsyncSenderElements.empty()) {
        m_writeInProgress = false;
    }
    else {
        boost::asio::async_write(*m_tcpSocketPtr, m_queueTcpAsyncSenderElements.front()->m_constBufferVec,
            boost::bind(&TcpAsyncSender::HandleTcpSend, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}

#ifdef OPENSSL_SUPPORT_ENABLED
TcpAsyncSenderSsl::TcpAsyncSenderSsl(ssl_stream_sharedptr_t & sslStreamSharedPtr, boost::asio::io_service & ioServiceRef) :
    m_ioServiceRef(ioServiceRef),
    m_sslStreamSharedPtr(sslStreamSharedPtr),
    m_writeInProgress(false)
{

}

TcpAsyncSenderSsl::~TcpAsyncSenderSsl() {

}

void TcpAsyncSenderSsl::AsyncSendSecure_NotThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    m_queueTcpAsyncSenderElements.push(std::unique_ptr<TcpAsyncSenderElement>(senderElementNeedingDeleted));
    if (!m_writeInProgress) {
        m_writeInProgress = true;
        boost::asio::async_write(*m_sslStreamSharedPtr, senderElementNeedingDeleted->m_constBufferVec,
            boost::bind(&TcpAsyncSenderSsl::HandleTcpSendSecure, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}

void TcpAsyncSenderSsl::AsyncSendSecure_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    boost::asio::post(m_ioServiceRef, boost::bind(&TcpAsyncSenderSsl::AsyncSendSecure_NotThreadSafe, this, senderElementNeedingDeleted));
}


void TcpAsyncSenderSsl::HandleTcpSendSecure(const boost::system::error_code& error, std::size_t bytes_transferred) {
    m_queueTcpAsyncSenderElements.front()->DoCallback(error, bytes_transferred);
    m_queueTcpAsyncSenderElements.pop();
    if (error) {
        std::cerr << "error in TcpAsyncSenderSsl::HandleTcpSendSecure: " << error.message() << std::endl;
    }
    else if (m_queueTcpAsyncSenderElements.empty()) {
        m_writeInProgress = false;
    }
    else {
        boost::asio::async_write(*m_sslStreamSharedPtr, m_queueTcpAsyncSenderElements.front()->m_constBufferVec,
            boost::bind(&TcpAsyncSenderSsl::HandleTcpSendSecure, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}

void TcpAsyncSenderSsl::AsyncSendUnsecure_NotThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    m_queueTcpAsyncSenderElements.push(std::unique_ptr<TcpAsyncSenderElement>(senderElementNeedingDeleted));
    if (!m_writeInProgress) {
        m_writeInProgress = true;
        //lowest_layer does not compile https://stackoverflow.com/a/32584870
        boost::asio::async_write(m_sslStreamSharedPtr->next_layer(), senderElementNeedingDeleted->m_constBufferVec, //https://stackoverflow.com/a/4726475
            boost::bind(&TcpAsyncSenderSsl::HandleTcpSendUnsecure, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}

void TcpAsyncSenderSsl::AsyncSendUnsecure_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    boost::asio::post(m_ioServiceRef, boost::bind(&TcpAsyncSenderSsl::AsyncSendUnsecure_NotThreadSafe, this, senderElementNeedingDeleted));
}


void TcpAsyncSenderSsl::HandleTcpSendUnsecure(const boost::system::error_code& error, std::size_t bytes_transferred) {
    m_queueTcpAsyncSenderElements.front()->DoCallback(error, bytes_transferred);
    m_queueTcpAsyncSenderElements.pop();
    if (error) {
        std::cerr << "error in TcpAsyncSenderSsl::HandleTcpSendUnsecure: " << error.message() << std::endl;
    }
    else if (m_queueTcpAsyncSenderElements.empty()) {
        m_writeInProgress = false;
    }
    else {
        boost::asio::async_write(m_sslStreamSharedPtr->next_layer(), m_queueTcpAsyncSenderElements.front()->m_constBufferVec,
            boost::bind(&TcpAsyncSenderSsl::HandleTcpSendUnsecure, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}
#endif
