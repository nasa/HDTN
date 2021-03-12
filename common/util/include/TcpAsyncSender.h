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
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <boost/function.hpp>
#include <zmq.hpp>

//#define TCP_ASYNC_SENDER_USE_VECTOR 1
struct TcpAsyncSenderElement {
    typedef boost::function<void(const boost::system::error_code& error, std::size_t bytes_transferred)> OnSuccessfulSendCallbackByIoServiceThread_t;
    TcpAsyncSenderElement();
    static void Create(std::unique_ptr<TcpAsyncSenderElement> & el,
        std::unique_ptr<std::vector<boost::uint8_t> > && data1,
        std::unique_ptr<std::vector<boost::uint8_t> > && data2,
        OnSuccessfulSendCallbackByIoServiceThread_t * onSuccessfulSendCallbackByIoServiceThreadPtr);
    static void Create(std::unique_ptr<TcpAsyncSenderElement> & el,
        std::unique_ptr<std::vector<boost::uint8_t> > && dataVector,
        std::unique_ptr<zmq::message_t> && dataZmq,
        OnSuccessfulSendCallbackByIoServiceThread_t * onSuccessfulSendCallbackByIoServiceThreadPtr);
    static void Create(std::unique_ptr<TcpAsyncSenderElement> & el,
        std::unique_ptr<std::vector<boost::uint8_t> > && data1,
        OnSuccessfulSendCallbackByIoServiceThread_t * onSuccessfulSendCallbackByIoServiceThreadPtr);

    void DoCallback(const boost::system::error_code& error, std::size_t bytes_transferred);

    std::vector<boost::asio::const_buffer> m_constBufferVec;
#ifdef TCP_ASYNC_SENDER_USE_VECTOR
    std::vector<std::unique_ptr<std::vector<boost::uint8_t> > > m_underlyingData;
#else
    std::unique_ptr<std::vector<boost::uint8_t> >  m_underlyingData[2];
#endif // TCP_ASYNC_SENDER_USE_VECTOR
    std::unique_ptr<zmq::message_t>  m_underlyingDataZmq;
    OnSuccessfulSendCallbackByIoServiceThread_t * m_onSuccessfulSendCallbackByIoServiceThreadPtr;
};

class TcpAsyncSender {
private:
    TcpAsyncSender();
public:
    
    TcpAsyncSender(const unsigned int circularBufferSize, boost::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr);

    ~TcpAsyncSender();
    bool AsyncSend_NotThreadSafe(std::unique_ptr<TcpAsyncSenderElement> && senderElement);
    bool AsyncSend_ThreadSafe(std::unique_ptr<TcpAsyncSenderElement> && senderElement);
    //void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
private:
    void PopCbThreadFunc();
    void HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred, const unsigned int readIndex);



    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    const unsigned int M_CIRCULAR_BUFFER_SIZE;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::unique_ptr<TcpAsyncSenderElement> > m_circularBufferElementsVec;
    
    volatile bool m_running;
    volatile bool m_writeInProgress;
    std::unique_ptr<boost::thread> m_threadCbReaderPtr;
    boost::condition_variable m_conditionVariableCb;
    boost::mutex m_mutex;
};



#endif //_TCP_ASYNC_SENDER_H
