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

void TcpAsyncSenderElement::Create(std::unique_ptr<TcpAsyncSenderElement> & el,
    std::unique_ptr<std::vector<boost::uint8_t> > && data1,
    std::unique_ptr<std::vector<boost::uint8_t> > && data2,
    OnSuccessfulSendCallbackByIoServiceThread_t * onSuccessfulSendCallbackByIoServiceThreadPtr)
{
    el = boost::make_unique<TcpAsyncSenderElement>();

    el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = onSuccessfulSendCallbackByIoServiceThreadPtr;

    el->m_constBufferVec.reserve(2);
    el->m_constBufferVec.push_back(boost::asio::buffer(*data1));
    el->m_constBufferVec.push_back(boost::asio::buffer(*data2));

    //https://stackoverflow.com/questions/54951635/why-to-use-stdmove-despite-the-parameter-is-an-r-value-reference
#ifdef TCP_ASYNC_SENDER_USE_VECTOR
    el->m_underlyingData.reserve(2);
    el->m_underlyingData.push_back(std::move(data1));
    el->m_underlyingData.push_back(std::move(data2));
#else
    el->m_underlyingData[0] = std::move(data1);
    el->m_underlyingData[1] = std::move(data2);
#endif // TCP_ASYNC_SENDER_USE_VECTOR


    //bytes_transferred = sock.send(bufs2);
}

void TcpAsyncSenderElement::Create(std::unique_ptr<TcpAsyncSenderElement> & el,
    std::unique_ptr<std::vector<boost::uint8_t> > && dataVector,
    std::unique_ptr<zmq::message_t> && dataZmq,
    OnSuccessfulSendCallbackByIoServiceThread_t * onSuccessfulSendCallbackByIoServiceThreadPtr)
{
    el = boost::make_unique<TcpAsyncSenderElement>();

    el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = onSuccessfulSendCallbackByIoServiceThreadPtr;

    el->m_constBufferVec.reserve(2);
    el->m_constBufferVec.push_back(boost::asio::buffer(*dataVector));
    el->m_constBufferVec.push_back(boost::asio::buffer(dataZmq->data(), dataZmq->size()));

    //https://stackoverflow.com/questions/54951635/why-to-use-stdmove-despite-the-parameter-is-an-r-value-reference
#ifdef TCP_ASYNC_SENDER_USE_VECTOR
    el->m_underlyingData.reserve(1);
    el->m_underlyingData.push_back(std::move(dataVector));
#else
    el->m_underlyingData[0] = std::move(dataVector);
#endif // TCP_ASYNC_SENDER_USE_VECTOR
    el->m_underlyingDataZmq = std::move(dataZmq);

    //bytes_transferred = sock.send(bufs2);
}

void TcpAsyncSenderElement::Create(std::unique_ptr<TcpAsyncSenderElement> & el,
    std::unique_ptr<std::vector<boost::uint8_t> > && data1,
    OnSuccessfulSendCallbackByIoServiceThread_t * onSuccessfulSendCallbackByIoServiceThreadPtr)
{
    el = boost::make_unique<TcpAsyncSenderElement>();

    el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = onSuccessfulSendCallbackByIoServiceThreadPtr;

    el->m_constBufferVec.reserve(1);
    el->m_constBufferVec.push_back(boost::asio::buffer(*data1));

    //https://stackoverflow.com/questions/54951635/why-to-use-stdmove-despite-the-parameter-is-an-r-value-reference
#ifdef TCP_ASYNC_SENDER_USE_VECTOR
    el->m_underlyingData.reserve(1);
    el->m_underlyingData.push_back(std::move(data1));
#else
    el->m_underlyingData[0] = std::move(data1);
#endif // TCP_ASYNC_SENDER_USE_VECTOR
}

void TcpAsyncSenderElement::Create(std::unique_ptr<TcpAsyncSenderElement> & el,
    const uint8_t * staticData, std::size_t staticDataSize,
    OnSuccessfulSendCallbackByIoServiceThread_t * onSuccessfulSendCallbackByIoServiceThreadPtr)
{
    el = boost::make_unique<TcpAsyncSenderElement>();

    el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = onSuccessfulSendCallbackByIoServiceThreadPtr;

    el->m_constBufferVec.reserve(1);
    el->m_constBufferVec.push_back(boost::asio::buffer(staticData, staticDataSize));
}


TcpAsyncSender::TcpAsyncSender(const unsigned int circularBufferSize, boost::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr) :
m_tcpSocketPtr(tcpSocketPtr),
M_CIRCULAR_BUFFER_SIZE(circularBufferSize),
m_circularIndexBuffer(M_CIRCULAR_BUFFER_SIZE),
m_circularBufferElementsVec(M_CIRCULAR_BUFFER_SIZE),
m_running(false),
m_writeInProgress(false)
{
    m_running = true;
    m_threadCbReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&TcpAsyncSender::PopCbThreadFunc, this)); //create and start the worker thread
}

TcpAsyncSender::~TcpAsyncSender() {
    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr.reset(); //delete
    }
}

bool TcpAsyncSender::AsyncSend_NotThreadSafe(std::unique_ptr<TcpAsyncSenderElement> && senderElement) {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == UINT32_MAX) {
        std::cerr << "critical error in TcpAsyncSender::AsyncSend(): buffers full\n";
        return false;
    }
    m_circularBufferElementsVec[writeIndex] = std::move(senderElement);
    m_circularIndexBuffer.CommitWrite();
    m_conditionVariableCb.notify_one();
    return true;
}

bool TcpAsyncSender::AsyncSend_ThreadSafe(std::unique_ptr<TcpAsyncSenderElement> && senderElement) {
    boost::mutex::scoped_lock lock(m_mutex);
    return AsyncSend_NotThreadSafe(std::move(senderElement));
}

void TcpAsyncSender::PopCbThreadFunc() {
    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (m_circularIndexBuffer.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty
        
        if (m_writeInProgress) { //if empty
            m_conditionVariableCb.timed_wait(lock, boost::posix_time::milliseconds(100)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }

        const unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
        if (consumeIndex == UINT32_MAX) { //if empty
            m_conditionVariableCb.timed_wait(lock, boost::posix_time::milliseconds(100)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        m_writeInProgress = true;
        boost::asio::async_write(*m_tcpSocketPtr, m_circularBufferElementsVec[consumeIndex]->m_constBufferVec,
            boost::bind(&TcpAsyncSender::HandleTcpSend, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                consumeIndex));        
    }

    std::cout << "TcpAsyncSender Circular buffer reader thread exiting\n";

}



void TcpAsyncSender::HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred, const unsigned int readIndex) {
    if (error) {
        std::cerr << "error in TcpAsyncSender::HandleTcpSend: " << error.message() << std::endl;
        return;
    }
    m_circularBufferElementsVec[readIndex]->DoCallback(error, bytes_transferred);
    m_circularBufferElementsVec[readIndex].reset(); //delete underlying data
    m_circularIndexBuffer.CommitRead();
    m_writeInProgress = false; //must be done after circular buffer read
    m_conditionVariableCb.notify_one();
}

