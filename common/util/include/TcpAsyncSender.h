#ifndef _TCP_ASYNC_SENDER_H
#define _TCP_ASYNC_SENDER_H 1
/*
A class that exists because according to:
    https://www.boost.org/doc/libs/1_75_0/doc/html/boost_asio/reference/async_write/overload1.html

Regarding the use of async_write, it says:

This operation is implemented in terms of zero or more calls to the stream's async_write_some function,
and is known as a composed operation. The program must ensure that the stream performs no other write
operations (such as async_write, the stream's async_write_some function, or any other composed operations
that perform writes) until this operation completes.

*/

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <queue>
#include <boost/function.hpp>
#include <zmq.hpp>

struct TcpAsyncSenderElement {
    typedef boost::function<void(const boost::system::error_code& error, std::size_t bytes_transferred)> OnSuccessfulSendCallbackByIoServiceThread_t;
    TcpAsyncSenderElement();
    
    void DoCallback(const boost::system::error_code& error, std::size_t bytes_transferred);

    std::vector<boost::asio::const_buffer> m_constBufferVec;
    std::vector<std::vector<boost::uint8_t> > m_underlyingData;
    std::unique_ptr<zmq::message_t>  m_underlyingDataZmq;
    OnSuccessfulSendCallbackByIoServiceThread_t * m_onSuccessfulSendCallbackByIoServiceThreadPtr;
};

class TcpAsyncSender {
private:
    TcpAsyncSender();
public:
    
    TcpAsyncSender(boost::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr, boost::asio::io_service & ioServiceRef);

    ~TcpAsyncSender();
    
    void AsyncSend_NotThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted);
    void AsyncSend_ThreadSafe(TcpAsyncSenderElement * senderElementNeedingDeleted);
    //void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
private:
    
    void HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);


    boost::asio::io_service & m_ioServiceRef;
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    std::queue<std::unique_ptr<TcpAsyncSenderElement> > m_queueTcpAsyncSenderElements;

    
    volatile bool m_writeInProgress;

};



#endif //_TCP_ASYNC_SENDER_H
