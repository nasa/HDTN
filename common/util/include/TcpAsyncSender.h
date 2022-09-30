/**
 * @file TcpAsyncSender.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 *
 * These TcpAsyncSender/TcpAsyncSenderSsl classes in conjuction with their data (class TcpAsyncSenderElement)
 * provide a means to ensure TCP or SSL sockets using boost::asio::write fully complete
 * and that no other writes occur during the write
 * and that the data remains valid during the write.
 * This class exists because according to:
 * https://www.boost.org/doc/libs/1_75_0/doc/html/boost_asio/reference/async_write/overload1.html
 *
 * Regarding the use of async_write, it says:
 *
 * This operation is implemented in terms of zero or more calls to the stream's async_write_some function,
 * and is known as a composed operation. The program must ensure that the stream performs no other write
 * operations (such as async_write, the stream's async_write_some function, or any other composed operations
 * that perform writes) until this operation completes.
 */

#ifndef _TCP_ASYNC_SENDER_H
#define _TCP_ASYNC_SENDER_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <queue>
#include <memory>
#include <boost/function.hpp>
#include <zmq.hpp>
#include "BundleCallbackFunctionDefines.h"
#ifdef OPENSSL_SUPPORT_ENABLED
#include <boost/asio/ssl.hpp>
#endif
#include "hdtn_util_export.h"

struct TcpAsyncSenderElement {
    typedef boost::function<void(const boost::system::error_code& error, std::size_t bytes_transferred, TcpAsyncSenderElement * elPtr)> OnSuccessfulSendCallbackByIoServiceThread_t;
    HDTN_UTIL_EXPORT TcpAsyncSenderElement();
    HDTN_UTIL_EXPORT ~TcpAsyncSenderElement();
    
    HDTN_UTIL_EXPORT void DoCallback(const boost::system::error_code& error, std::size_t bytes_transferred);

    std::vector<uint8_t> m_userData;
    std::vector<boost::asio::const_buffer> m_constBufferVec;
    std::vector<std::vector<boost::uint8_t> > m_underlyingDataVecHeaders;
    std::vector<boost::uint8_t> m_underlyingDataVecBundle;
    std::unique_ptr<zmq::message_t> m_underlyingDataZmqBundle;
    OnSuccessfulSendCallbackByIoServiceThread_t * m_onSuccessfulSendCallbackByIoServiceThreadPtr;
};

class TcpAsyncSender {
private:
    TcpAsyncSender();
public:
    
    HDTN_UTIL_EXPORT TcpAsyncSender(std::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr, boost::asio::io_service & ioServiceRef);

    HDTN_UTIL_EXPORT ~TcpAsyncSender();
    
    HDTN_UTIL_EXPORT void AsyncSend_NotThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted);
    HDTN_UTIL_EXPORT void AsyncSend_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted);

    HDTN_UTIL_EXPORT void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    HDTN_UTIL_EXPORT void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    HDTN_UTIL_EXPORT void SetUserAssignedUuid(uint64_t userAssignedUuid);
private:
    HDTN_UTIL_EXPORT void DoFailedBundleCallback(std::unique_ptr<TcpAsyncSenderElement> & el);
    
    HDTN_UTIL_EXPORT void HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);


    boost::asio::io_service & m_ioServiceRef;
    std::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    std::queue<std::unique_ptr<TcpAsyncSenderElement> > m_queueTcpAsyncSenderElements;

    
    volatile bool m_writeInProgress;
    volatile bool m_sendErrorOccurred;

    OnFailedBundleVecSendCallback_t m_onFailedBundleVecSendCallback;
    OnFailedBundleZmqSendCallback_t m_onFailedBundleZmqSendCallback;
    uint64_t m_userAssignedUuid;

};

#ifdef OPENSSL_SUPPORT_ENABLED
class TcpAsyncSenderSsl {
private:
    TcpAsyncSenderSsl();
public:
    typedef std::shared_ptr< boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > ssl_stream_sharedptr_t;

    HDTN_UTIL_EXPORT TcpAsyncSenderSsl(ssl_stream_sharedptr_t & sslStreamSharedPtr, boost::asio::io_service & ioServiceRef);

    HDTN_UTIL_EXPORT ~TcpAsyncSenderSsl();

    HDTN_UTIL_EXPORT void AsyncSendSecure_NotThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted);
    HDTN_UTIL_EXPORT void AsyncSendSecure_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted);
    HDTN_UTIL_EXPORT void AsyncSendUnsecure_NotThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted);
    HDTN_UTIL_EXPORT void AsyncSendUnsecure_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted);

    HDTN_UTIL_EXPORT void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    HDTN_UTIL_EXPORT void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    HDTN_UTIL_EXPORT void SetUserAssignedUuid(uint64_t userAssignedUuid);
private:
    HDTN_UTIL_EXPORT void DoFailedBundleCallback(std::unique_ptr<TcpAsyncSenderElement>& el);

    HDTN_UTIL_EXPORT void HandleTcpSendSecure(const boost::system::error_code& error, std::size_t bytes_transferred);
    HDTN_UTIL_EXPORT void HandleTcpSendUnsecure(const boost::system::error_code& error, std::size_t bytes_transferred);


    boost::asio::io_service & m_ioServiceRef;
    ssl_stream_sharedptr_t m_sslStreamSharedPtr;
    std::queue<std::unique_ptr<TcpAsyncSenderElement> > m_queueTcpAsyncSenderElements;


    volatile bool m_writeInProgress;
    volatile bool m_sendErrorOccurred;

    OnFailedBundleVecSendCallback_t m_onFailedBundleVecSendCallback;
    OnFailedBundleZmqSendCallback_t m_onFailedBundleZmqSendCallback;
    uint64_t m_userAssignedUuid;

};
#endif

#endif //_TCP_ASYNC_SENDER_H
