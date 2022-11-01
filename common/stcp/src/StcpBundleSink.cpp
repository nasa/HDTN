/**
 * @file StcpBundleSink.cpp
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

#include <boost/bind/bind.hpp>
#include <memory>
#include "Logger.h"
#include "StcpBundleSink.h"
#include <boost/endian/conversion.hpp>
#include <boost/make_unique.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

StcpBundleSink::StcpBundleSink(std::shared_ptr<boost::asio::ip::tcp::socket> tcpSocketPtr,
    boost::asio::io_service & tcpSocketIoServiceRef,
    const WholeBundleReadyCallback_t & wholeBundleReadyCallback,
    const unsigned int numCircularBufferVectors,
    const uint64_t maxBundleSizeBytes,
    const NotifyReadyToDeleteCallback_t & notifyReadyToDeleteCallback) :

    m_wholeBundleReadyCallback(wholeBundleReadyCallback),
    m_notifyReadyToDeleteCallback(notifyReadyToDeleteCallback),
    m_tcpSocketPtr(tcpSocketPtr),
    m_tcpSocketIoServiceRef(tcpSocketIoServiceRef),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numCircularBufferVectors),
    M_MAX_BUNDLE_SIZE_BYTES(maxBundleSizeBytes),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBytesTransferredCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_stateTcpReadActive(false),
    m_printedCbTooSmallNotice(false),
    m_running(false),
    m_safeToDelete(false)
{
    LOG_INFO(subprocess) << "stcp sink using CB size: " << M_NUM_CIRCULAR_BUFFER_VECTORS;
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
        if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                LOG_WARNING(subprocess) << "StcpBundleSink::TryStartTcpReceive(): buffers full.. you might want to increase the circular buffer size!"; 
            }
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
            LOG_INFO(subprocess) << "keepalive packet received";
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
            if (m_incomingBundleSize > M_MAX_BUNDLE_SIZE_BYTES) { //SAFETY CHECKS ON SIZE BEFORE ALLOCATE
                LOG_FATAL(subprocess) << "StcpBundleSink::HandleTcpReceiveIncomingBundleSize(): size " << m_incomingBundleSize << " exceeds 100MB.. TCP receiving on StcpBundleSink will now stop!";
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
        LOG_INFO(subprocess) << "Tcp connection closed cleanly by peer";
        DoStcpShutdown();
    }
    else if (error != boost::asio::error::operation_aborted) {
        LOG_ERROR(subprocess) << "StcpBundleSink::HandleTcpReceiveIncomingBundleSize: " << error.message();
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
            LOG_ERROR(subprocess) << "StcpBundleSink::HandleTcpReceiveBundleData: bytesTransferred ("
                << bytesTransferred << ") != m_incomingBundleSize (" << m_incomingBundleSize << ")";
        }
    }
    else if (error == boost::asio::error::eof) {
        LOG_INFO(subprocess) << "Tcp connection closed cleanly by peer";
        DoStcpShutdown();
    }
    else if (error != boost::asio::error::operation_aborted) {
        LOG_ERROR(subprocess) << "StcpBundleSink::HandleTcpReceiveBundleData: " << error.message();
    }
}



void StcpBundleSink::PopCbThreadFunc() {

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (m_circularIndexBuffer.GetIndexForRead() != CIRCULAR_INDEX_BUFFER_EMPTY)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
        boost::asio::post(m_tcpSocketIoServiceRef, boost::bind(&StcpBundleSink::TryStartTcpReceive, this)); //keep this a thread safe operation by letting ioService thread run it
        if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty
            m_conditionVariableCb.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        m_wholeBundleReadyCallback(m_tcpReceiveBuffersCbVec[consumeIndex]);
        
        m_circularIndexBuffer.CommitRead();
    }

    LOG_INFO(subprocess) << "StcpBundleSink Circular buffer reader thread exiting";

}

void StcpBundleSink::DoStcpShutdown() {
    boost::asio::post(m_tcpSocketIoServiceRef, boost::bind(&StcpBundleSink::HandleSocketShutdown, this));
}

void StcpBundleSink::HandleSocketShutdown() {
    //final code to shut down tcp sockets
    if (m_tcpSocketPtr) {
        if (m_tcpSocketPtr->is_open()) {
            try {
                LOG_INFO(subprocess) << "shutting down StcpBundleSink TCP socket..";
                m_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
            }
            catch (const boost::system::system_error & e) {
                LOG_ERROR(subprocess) << "StcpBundleSink::HandleSocketShutdown: " << e.what();
            }
            try {
                LOG_INFO(subprocess) << "closing StcpBundleSink TCP socket socket..";
                m_tcpSocketPtr->close();
            }
            catch (const boost::system::system_error & e) {
                LOG_ERROR(subprocess) << "StcpBundleSink::HandleSocketShutdown: " << e.what();
            }
        }
        LOG_INFO(subprocess) << "deleting TcpclBundleSink TCP Socket";
        if (m_tcpSocketPtr.use_count() != 1) {
            LOG_ERROR(subprocess) << "m_tcpSocketPtr.use_count() != 1";
        }
        m_tcpSocketPtr = std::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_safeToDelete = true;
    if (m_notifyReadyToDeleteCallback) {
        m_notifyReadyToDeleteCallback();
    }
}

bool StcpBundleSink::ReadyToBeDeleted() {
    return m_safeToDelete;
}
