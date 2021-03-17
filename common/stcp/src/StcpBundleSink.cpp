#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "StcpBundleSink.h"
#include <boost/endian/conversion.hpp>
#include <boost/make_unique.hpp>

StcpBundleSink::StcpBundleSink(boost::shared_ptr<boost::asio::ip::tcp::socket> tcpSocketPtr,
                                 WholeBundleReadyCallback_t wholeBundleReadyCallback,
                                 //ConnectionClosedCallback_t connectionClosedCallback,
                                 const unsigned int numCircularBufferVectors) :
    m_wholeBundleReadyCallback(wholeBundleReadyCallback),
    m_tcpSocketPtr(tcpSocketPtr),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numCircularBufferVectors),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBytesTransferredCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_running(false),
    m_safeToDelete(false)
{

    m_running = true;
    m_threadCbReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&StcpBundleSink::PopCbThreadFunc, this)); //create and start the worker thread
   
    StartTcpReceiveIncomingBundleSize();
}

StcpBundleSink::~StcpBundleSink() {

    DoStcpShutdown();
    
    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr.reset(); //delete it
    }
}


void StcpBundleSink::StartTcpReceiveIncomingBundleSize() {
    if (m_tcpSocketPtr) {
        boost::asio::async_read(*m_tcpSocketPtr,
            boost::asio::buffer(&m_incomingBundleSize, sizeof(m_incomingBundleSize)),
            boost::bind(&StcpBundleSink::HandleTcpReceiveIncomingBundleSize, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    }
}
void StcpBundleSink::HandleTcpReceiveIncomingBundleSize(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        if (m_incomingBundleSize == 0) { //keepalive (0 is endian agnostic)
            std::cout << "notice: keepalive packet received" << std::endl;
            StartTcpReceiveIncomingBundleSize();
        }
        else {
            boost::endian::big_to_native_inplace(m_incomingBundleSize);
            StartTcpReceiveBundleData(); //continue operation only if there was no error
        }
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "Tcp connection closed cleanly by peer" << std::endl;
        DoStcpShutdown();
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "Error in StcpBundleSink::HandleTcpReceiveIncomingBundleSize: " << error.message() << std::endl;
    }
}
void StcpBundleSink::StartTcpReceiveBundleData() {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == UINT32_MAX) {
        std::cerr << "critical error in StcpBundleSink::StartTcpReceiveBundleData(): buffers full.. TCP receiving on StcpBundleSink will now stop!" << std::endl;
        DoStcpShutdown();
        return;
    }
    if (m_tcpSocketPtr) {
        m_tcpReceiveBuffersCbVec[writeIndex].resize(m_incomingBundleSize);
        boost::asio::async_read(*m_tcpSocketPtr,
            boost::asio::buffer(m_tcpReceiveBuffersCbVec[writeIndex]),
            boost::bind(&StcpBundleSink::HandleTcpReceiveBundleData, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred,
                        writeIndex));
    }
}
void StcpBundleSink::HandleTcpReceiveBundleData(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    if (!error) {
        if (bytesTransferred == m_incomingBundleSize) {
            m_tcpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
            m_circularIndexBuffer.CommitWrite(); //write complete at this point
            m_conditionVariableCb.notify_one();
            StartTcpReceiveIncomingBundleSize(); //restart operation only if there was no error
        }
        else {
            std::cerr << "Critical error in StcpBundleSink::HandleTcpReceiveBundleData: bytesTransferred ("
                << bytesTransferred << ") != m_incomingBundleSize (" << m_incomingBundleSize << ")" << std::endl;
        }
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "Tcp connection closed cleanly by peer" << std::endl;
        DoStcpShutdown();
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "Error in StcpBundleSink::HandleTcpReceiveBundleData: " << error.message() << std::endl;
    }
}



void StcpBundleSink::PopCbThreadFunc() {

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (m_circularIndexBuffer.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile

        if (consumeIndex == UINT32_MAX) { //if empty
            m_conditionVariableCb.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        m_wholeBundleReadyCallback(m_tcpReceiveBuffersCbVec[consumeIndex]);
        
        m_circularIndexBuffer.CommitRead();
    }

    std::cout << "StcpBundleSink Circular buffer reader thread exiting\n";

}



void StcpBundleSink::DoStcpShutdown() {
    //final code to shut down tcp sockets
    if (m_tcpSocketPtr && m_tcpSocketPtr->is_open()) {
        try {
            std::cout << "shutting down StcpBundleSink TCP socket.." << std::endl;
            m_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in StcpBundleSink::DoStcpShutdown: " << e.what() << std::endl;
        }
        try {
            std::cout << "closing StcpBundleSink TCP socket socket.." << std::endl;
            m_tcpSocketPtr->close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in StcpBundleSink::DoStcpShutdown: " << e.what() << std::endl;
        }
        //don't delete the tcp socket because the Destructor may call this from another thread
        //so prevent a race condition that would cause a null pointer exception
        //std::cout << "deleting tcp socket" << std::endl;
        //m_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_safeToDelete = true;
}

bool StcpBundleSink::ReadyToBeDeleted() {
    return m_safeToDelete;
}
