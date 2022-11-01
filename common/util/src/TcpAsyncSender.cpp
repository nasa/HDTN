/**
 * @file TcpAsyncSender.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <string>
#include "TcpAsyncSender.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>

TcpAsyncSenderElement::TcpAsyncSenderElement() : m_onSuccessfulSendCallbackByIoServiceThreadPtr(NULL) {}
TcpAsyncSenderElement::~TcpAsyncSenderElement() {}

void TcpAsyncSenderElement::DoCallback(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if(m_onSuccessfulSendCallbackByIoServiceThreadPtr && (*m_onSuccessfulSendCallbackByIoServiceThreadPtr)) {
        (*m_onSuccessfulSendCallbackByIoServiceThreadPtr)(error, bytes_transferred, this);
    }
}


TcpAsyncSender::TcpAsyncSender(std::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr, boost::asio::io_service & ioServiceRef) :
    m_ioServiceRef(ioServiceRef),
    m_tcpSocketPtr(tcpSocketPtr),
    m_writeInProgress(false),
    m_sendErrorOccurred(false)
{

}

TcpAsyncSender::~TcpAsyncSender() {

}

void TcpAsyncSender::AsyncSend_NotThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    std::unique_ptr<TcpAsyncSenderElement> elUniquePtr(senderElementNeedingDeleted);
    if (m_sendErrorOccurred) {
        //prevent bundle from being queued
        DoFailedBundleCallback(elUniquePtr);
    }
    else {
        m_queueTcpAsyncSenderElements.push(std::move(elUniquePtr));
        if (!m_writeInProgress) {
            m_writeInProgress = true;
            boost::asio::async_write(*m_tcpSocketPtr, senderElementNeedingDeleted->m_constBufferVec,
                boost::bind(&TcpAsyncSender::HandleTcpSend, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }
}

void TcpAsyncSender::AsyncSend_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    boost::asio::post(m_ioServiceRef, boost::bind(&TcpAsyncSender::AsyncSend_NotThreadSafe, this, senderElementNeedingDeleted));
}


void TcpAsyncSender::HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred) {
    std::unique_ptr<TcpAsyncSenderElement> elPtr = std::move(m_queueTcpAsyncSenderElements.front());
    elPtr->DoCallback(error, bytes_transferred);
    m_queueTcpAsyncSenderElements.pop();
    if (error) {
        m_sendErrorOccurred = true;
        LOG_ERROR(hdtn::Logger::SubProcess::none) << "TcpAsyncSender::HandleTcpSend: " << error.message();
        //empty the queue
        DoFailedBundleCallback(elPtr);
        while (!m_queueTcpAsyncSenderElements.empty()) {
            DoFailedBundleCallback(m_queueTcpAsyncSenderElements.front());
            m_queueTcpAsyncSenderElements.pop();
        }
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

void TcpAsyncSender::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_onFailedBundleVecSendCallback = callback;
}
void TcpAsyncSender::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_onFailedBundleZmqSendCallback = callback;
}
void TcpAsyncSender::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_userAssignedUuid = userAssignedUuid;
}
void TcpAsyncSender::DoFailedBundleCallback(std::unique_ptr<TcpAsyncSenderElement>& el) {
    if ((el->m_underlyingDataVecBundle.size()) && (m_onFailedBundleVecSendCallback)) {
        m_onFailedBundleVecSendCallback(el->m_underlyingDataVecBundle, el->m_userData, m_userAssignedUuid);
    }
    else if ((el->m_underlyingDataZmqBundle) && (m_onFailedBundleZmqSendCallback)) {
        m_onFailedBundleZmqSendCallback(*(el->m_underlyingDataZmqBundle), el->m_userData, m_userAssignedUuid);
    }
}

#ifdef OPENSSL_SUPPORT_ENABLED
TcpAsyncSenderSsl::TcpAsyncSenderSsl(ssl_stream_sharedptr_t & sslStreamSharedPtr, boost::asio::io_service & ioServiceRef) :
    m_ioServiceRef(ioServiceRef),
    m_sslStreamSharedPtr(sslStreamSharedPtr),
    m_writeInProgress(false),
    m_sendErrorOccurred(false)
{

}

TcpAsyncSenderSsl::~TcpAsyncSenderSsl() {

}

void TcpAsyncSenderSsl::AsyncSendSecure_NotThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    std::unique_ptr<TcpAsyncSenderElement> elUniquePtr(senderElementNeedingDeleted);
    if (m_sendErrorOccurred) {
        //prevent bundle from being queued
        DoFailedBundleCallback(elUniquePtr);
    }
    else {
        m_queueTcpAsyncSenderElements.push(std::move(elUniquePtr));
        if (!m_writeInProgress) {
            m_writeInProgress = true;
            boost::asio::async_write(*m_sslStreamSharedPtr, senderElementNeedingDeleted->m_constBufferVec,
                boost::bind(&TcpAsyncSenderSsl::HandleTcpSendSecure, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }
}

void TcpAsyncSenderSsl::AsyncSendSecure_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    boost::asio::post(m_ioServiceRef, boost::bind(&TcpAsyncSenderSsl::AsyncSendSecure_NotThreadSafe, this, senderElementNeedingDeleted));
}


void TcpAsyncSenderSsl::HandleTcpSendSecure(const boost::system::error_code& error, std::size_t bytes_transferred) {
    std::unique_ptr<TcpAsyncSenderElement> elPtr = std::move(m_queueTcpAsyncSenderElements.front());
    elPtr->DoCallback(error, bytes_transferred);
    m_queueTcpAsyncSenderElements.pop();
    if (error) {
        m_sendErrorOccurred = true;
        LOG_ERROR(hdtn::Logger::SubProcess::none) << "error in TcpAsyncSenderSsl::HandleTcpSendSecure: " << error.message();
        //empty the queue
        DoFailedBundleCallback(elPtr);
        while (!m_queueTcpAsyncSenderElements.empty()) {
            DoFailedBundleCallback(m_queueTcpAsyncSenderElements.front());
            m_queueTcpAsyncSenderElements.pop();
        }
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
    std::unique_ptr<TcpAsyncSenderElement> elUniquePtr(senderElementNeedingDeleted);
    if (m_sendErrorOccurred) {
        //prevent bundle from being queued
        DoFailedBundleCallback(elUniquePtr);
    }
    else {
        m_queueTcpAsyncSenderElements.push(std::move(elUniquePtr));
        if (!m_writeInProgress) {
            m_writeInProgress = true;
            //lowest_layer does not compile https://stackoverflow.com/a/32584870
            boost::asio::async_write(m_sslStreamSharedPtr->next_layer(), senderElementNeedingDeleted->m_constBufferVec, //https://stackoverflow.com/a/4726475
                boost::bind(&TcpAsyncSenderSsl::HandleTcpSendUnsecure, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }
}

void TcpAsyncSenderSsl::AsyncSendUnsecure_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted) {
    boost::asio::post(m_ioServiceRef, boost::bind(&TcpAsyncSenderSsl::AsyncSendUnsecure_NotThreadSafe, this, senderElementNeedingDeleted));
}


void TcpAsyncSenderSsl::HandleTcpSendUnsecure(const boost::system::error_code& error, std::size_t bytes_transferred) {
    std::unique_ptr<TcpAsyncSenderElement> elPtr = std::move(m_queueTcpAsyncSenderElements.front());
    elPtr->DoCallback(error, bytes_transferred);
    m_queueTcpAsyncSenderElements.pop();
    if (error) {
        m_sendErrorOccurred = true;
        LOG_ERROR(hdtn::Logger::SubProcess::none) << "error in TcpAsyncSenderSsl::HandleTcpSendUnsecure: " << error.message();
        //empty the queue
        DoFailedBundleCallback(elPtr);
        while (!m_queueTcpAsyncSenderElements.empty()) {
            DoFailedBundleCallback(m_queueTcpAsyncSenderElements.front());
            m_queueTcpAsyncSenderElements.pop();
        }
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

void TcpAsyncSenderSsl::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_onFailedBundleVecSendCallback = callback;
}
void TcpAsyncSenderSsl::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_onFailedBundleZmqSendCallback = callback;
}
void TcpAsyncSenderSsl::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_userAssignedUuid = userAssignedUuid;
}
void TcpAsyncSenderSsl::DoFailedBundleCallback(std::unique_ptr<TcpAsyncSenderElement>& el) {
    if ((el->m_underlyingDataVecBundle.size()) && (m_onFailedBundleVecSendCallback)) {
        m_onFailedBundleVecSendCallback(el->m_underlyingDataVecBundle, el->m_userData, m_userAssignedUuid);
    }
    else if ((el->m_underlyingDataZmqBundle) && (m_onFailedBundleZmqSendCallback)) {
        m_onFailedBundleZmqSendCallback(*(el->m_underlyingDataZmqBundle), el->m_userData, m_userAssignedUuid);
    }
}
#endif
