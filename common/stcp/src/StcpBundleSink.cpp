#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "StcpBundleSink.h"
#include <boost/endian/conversion.hpp>
#include <boost/make_unique.hpp>

StcpBundleSink::StcpBundleSink(boost::shared_ptr<boost::asio::ip::tcp::socket> tcpSocketPtr,
    boost::asio::io_service & tcpSocketIoServiceRef,
    const WholeBundleReadyCallback_t & wholeBundleReadyCallback,
    const unsigned int numCircularBufferVectors,
    const NotifyReadyToDeleteCallback_t & notifyReadyToDeleteCallback) :

    m_wholeBundleReadyCallback(wholeBundleReadyCallback),
    m_notifyReadyToDeleteCallback(notifyReadyToDeleteCallback),
    m_tcpSocketPtr(tcpSocketPtr),
    m_tcpSocketIoServiceRef(tcpSocketIoServiceRef),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numCircularBufferVectors),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBytesTransferredCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_stateTcpReadActive(false),
    m_running(false),
    m_safeToDelete(false)
{
    std::cout << "stcp sink using CB size: " << M_NUM_CIRCULAR_BUFFER_VECTORS << std::endl;
    m_running = true;
    m_threadCbReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&StcpBundleSink::PopCbThreadFunc, this)); //create and start the worker thread
   
    TryStartTcpReceive();
}

StcpBundleSink::~StcpBundleSink() {

    if (!m_safeToDelete) {
        DoStcpShutdown();
        while (!m_safeToDelete) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
        }
    }
    
    
    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr.reset(); //delete it
    }
}

//Note: the tcp layer will control flow in the event that the source is faster than the sink
void StcpBundleSink::TryStartTcpReceive() {
    if ((!m_stateTcpReadActive) && (m_tcpSocketPtr)) {

        const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == UINT32_MAX) {
            //std::cout << "notice in StcpBundleSink::StartTcpReceiveBundleData(): buffers full.. you might want to increase the circular buffer size!" << std::endl;            
        }
        else {
            //StartTcpReceiveIncomingBundleSize
            m_stateTcpReadActive = true;
            boost::asio::async_read(*m_tcpSocketPtr,
                boost::asio::buffer(&m_incomingBundleSize, sizeof(m_incomingBundleSize)),
                boost::bind(&StcpBundleSink::HandleTcpReceiveIncomingBundleSize, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    writeIndex));
            
        }
    }
}


void StcpBundleSink::HandleTcpReceiveIncomingBundleSize(const boost::system::error_code & error, std::size_t bytesTransferred, const unsigned int writeIndex) {
    if (!error) {
        if (m_incomingBundleSize == 0) { //keepalive (0 is endian agnostic)
            std::cout << "notice: keepalive packet received" << std::endl;
            //StartTcpReceiveIncomingBundleSize
            boost::asio::async_read(*m_tcpSocketPtr,
                boost::asio::buffer(&m_incomingBundleSize, sizeof(m_incomingBundleSize)),
                boost::bind(&StcpBundleSink::HandleTcpReceiveIncomingBundleSize, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    writeIndex));
        }
        else {
            boost::endian::big_to_native_inplace(m_incomingBundleSize);
            //continue operation StartTcpReceiveBundleData only if there was no error
            //TODO PUT CONFIG FILE LIMIT ON MAX SIZE
            if (m_incomingBundleSize > 100000000) { //SAFETY CHECKS ON SIZE BEFORE ALLOCATE (100MB max for now)
                std::cerr << "critical error in StcpBundleSink::HandleTcpReceiveIncomingBundleSize(): size " << m_incomingBundleSize << " exceeds 100MB.. TCP receiving on StcpBundleSink will now stop!" << std::endl;
                DoStcpShutdown(); //leave in m_stateTcpReadActive = true
            }
            else {
                m_tcpReceiveBuffersCbVec[writeIndex].resize(m_incomingBundleSize);
                boost::asio::async_read(*m_tcpSocketPtr,
                    boost::asio::buffer(m_tcpReceiveBuffersCbVec[writeIndex]),
                    boost::bind(&StcpBundleSink::HandleTcpReceiveBundleData, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred,
                        writeIndex));
            }
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

void StcpBundleSink::HandleTcpReceiveBundleData(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    if (!error) {
        if (bytesTransferred == m_incomingBundleSize) {
            m_tcpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
            m_circularIndexBuffer.CommitWrite(); //write complete at this point
            m_stateTcpReadActive = false; //must be false before calling TryStartTcpReceive
            m_conditionVariableCb.notify_one();
            TryStartTcpReceive(); //restart operation only if there was no error
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
        boost::asio::post(m_tcpSocketIoServiceRef, boost::bind(&StcpBundleSink::TryStartTcpReceive, this)); //keep this a thread safe operation by letting ioService thread run it
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
    boost::asio::post(m_tcpSocketIoServiceRef, boost::bind(&StcpBundleSink::HandleSocketShutdown, this));
}

void StcpBundleSink::HandleSocketShutdown() {
    //final code to shut down tcp sockets
    if (m_tcpSocketPtr) {
        if (m_tcpSocketPtr->is_open()) {
            try {
                std::cout << "shutting down StcpBundleSink TCP socket.." << std::endl;
                m_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
            }
            catch (const boost::system::system_error & e) {
                std::cerr << "error in StcpBundleSink::HandleSocketShutdown: " << e.what() << std::endl;
            }
            try {
                std::cout << "closing StcpBundleSink TCP socket socket.." << std::endl;
                m_tcpSocketPtr->close();
            }
            catch (const boost::system::system_error & e) {
                std::cerr << "error in StcpBundleSink::HandleSocketShutdown: " << e.what() << std::endl;
            }
        }
        std::cout << "deleting TcpclBundleSink TCP Socket" << std::endl;
        if (m_tcpSocketPtr.use_count() != 1) {
            std::cerr << "error m_tcpSocketPtr.use_count() != 1" << std::endl;
        }
        m_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_safeToDelete = true;
    if (m_notifyReadyToDeleteCallback) {
        m_notifyReadyToDeleteCallback();
    }
}

bool StcpBundleSink::ReadyToBeDeleted() {
    return m_safeToDelete;
}
