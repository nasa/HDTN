/**
 * @file UdpBundleSink.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
#include "UdpBundleSink.h"
#include "Logger.h"
#include <boost/endian/conversion.hpp>
#include <boost/make_unique.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

UdpBundleSink::UdpBundleSink(boost::asio::io_service & ioService,
    uint16_t udpPort,
    const WholeBundleReadyCallbackUdp_t & wholeBundleReadyCallback,
    const unsigned int numCircularBufferVectors,
    const unsigned int maxUdpPacketSizeBytes,
    const NotifyReadyToDeleteCallback_t & notifyReadyToDeleteCallback) :
    m_wholeBundleReadyCallback(wholeBundleReadyCallback),
    m_notifyReadyToDeleteCallback(notifyReadyToDeleteCallback),
    m_udpSocket(ioService),
    m_ioServiceRef(ioService),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numCircularBufferVectors),
    M_MAX_UDP_PACKET_SIZE_BYTES(maxUdpPacketSizeBytes),
    m_udpReceiveBuffer(M_MAX_UDP_PACKET_SIZE_BYTES),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_remoteEndpointsCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveBytesTransferredCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_running(false),
    m_safeToDelete(false),
    m_printedCbTooSmallNotice(false),
    //telemetry
    m_connectionName(),
    m_connectionNamePtr("null"),
    M_INPUT_NAME(std::string("*:") + boost::lexical_cast<std::string>(udpPort)),
    m_countCircularBufferOverruns(0),
    m_totalBundleBytesReceived(0),
    m_totalBundlesReceived(0)
{
    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(M_MAX_UDP_PACKET_SIZE_BYTES);
    }
    
    m_running = true;
    m_threadCbReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&UdpBundleSink::PopCbThreadFunc, this)); //create and start the worker thread

    //Receiver UDP
    try {
        m_udpSocket.open(boost::asio::ip::udp::v4());
        m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), udpPort));
    }
    catch (const boost::system::system_error & e) {
        LOG_ERROR(subprocess) << "Could not bind on UDP port " << udpPort;
        LOG_ERROR(subprocess) << e.what();

        m_mutexCb.lock();
        m_running = false; //thread stopping criteria
        m_mutexCb.unlock();
        m_conditionVariableCb.notify_one();

        return;
    }
    LOG_INFO(subprocess) << "UdpBundleSink bound successfully on UDP port " << udpPort << "...";
    StartUdpReceive(); //call before creating io_service thread so that it has "work"
}

UdpBundleSink::~UdpBundleSink() {

    if (!m_safeToDelete) {
        DoUdpShutdown();
        while (!m_safeToDelete) {
            try {
                boost::this_thread::sleep(boost::posix_time::milliseconds(250));
            }
            catch (const boost::thread_resource_error&) {}
            catch (const boost::thread_interrupted&) {}
            catch (const boost::condition_error&) {}
            catch (const boost::lock_error&) {}
        }
    }
    
    
    m_mutexCb.lock();
    m_running = false; //thread stopping criteria
    m_mutexCb.unlock();
    m_conditionVariableCb.notify_one();

    if (m_threadCbReaderPtr) {
        try {
            m_threadCbReaderPtr->join();
            m_threadCbReaderPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping UdpBundleSink threadCbReader";
        }
        //print stats once
        LOG_INFO(subprocess) << "UDP Bundle Sink / Induct Connection:"
            << "\n connectionName " << m_connectionNamePtr.load(std::memory_order_acquire)
            << "\n inputName " << M_INPUT_NAME
            << "\n totalStcpBytesReceived " << m_countCircularBufferOverruns
            << "\n totalBundleBytesReceived " << m_totalBundleBytesReceived
            << "\n totalBundlesReceived " << m_totalBundlesReceived;
    }
}

void UdpBundleSink::StartUdpReceive() {
    m_udpSocket.async_receive_from(
        boost::asio::buffer(m_udpReceiveBuffer),
        m_remoteEndpoint,
        boost::bind(&UdpBundleSink::HandleUdpReceive, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
}

void UdpBundleSink::HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
            m_countCircularBufferOverruns.fetch_add(1, std::memory_order_relaxed);
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                LOG_INFO(subprocess) << "LtpUdpEngine::StartUdpReceive(): buffers full.. you might want to increase the circular buffer size! This UDP packet will be dropped!";
            }
        }
        else {
            if (m_lastRemoteEndpoint != m_remoteEndpoint) {
                m_lastRemoteEndpoint = m_remoteEndpoint;
                if (m_connectionName.empty()) {
                    m_connectionName = m_remoteEndpoint.address().to_string()
                        + ":" + boost::lexical_cast<std::string>(m_remoteEndpoint.port());
                    m_connectionNamePtr.store(m_connectionName.c_str(), std::memory_order_release);
                }
                else {
                    m_connectionNamePtr.store("multi-src detected", std::memory_order_release);
                }
            }

            m_udpReceiveBuffer.swap(m_udpReceiveBuffersCbVec[writeIndex]);
            m_udpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
            m_remoteEndpointsCbVec[writeIndex] = std::move(m_remoteEndpoint);
            m_mutexCb.lock();
            m_circularIndexBuffer.CommitWrite(); //write complete at this point
            m_mutexCb.unlock();
            m_conditionVariableCb.notify_one();
        }
        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        LOG_FATAL(subprocess) << "UdpBundleSink::HandleUdpReceive(): " << error.message();
        DoUdpShutdown();
    }
}




void UdpBundleSink::PopCbThreadFunc() {
    ThreadNamer::SetThisThreadName("udpBundleSinkCbReader");

    while (true) { //keep thread alive if running or cb not empty, i.e. "while (m_running || (m_circularIndexBuffer.GetIndexForRead() != CIRCULAR_INDEX_BUFFER_EMPTY))"
        unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
        if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty
            //try again, but with the mutex
            boost::mutex::scoped_lock lock(m_mutexCb);
            consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
            if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty again (lock mutex (above) before checking condition)
                if (!m_running.load(std::memory_order_acquire)) { //m_running is mutex protected, if it stopped running, exit the thread (lock mutex (above) before checking condition)
                    break; //thread stopping criteria (empty and not running)
                }
                m_conditionVariableCb.wait(lock); // call lock.unlock() and blocks the current thread
                //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                continue;
            }
        }
        const std::size_t bytesTransferred = m_udpReceiveBytesTransferredCbVec[consumeIndex];
        m_totalBundleBytesReceived.fetch_add(bytesTransferred, std::memory_order_relaxed);
        m_totalBundlesReceived.fetch_add(1, std::memory_order_relaxed);
        //m_wholeBundleReadyCallback(m_udpReceiveBuffersCbVec[consumeIndex], m_udpReceiveBytesTransferredCbVec[consumeIndex]);
        m_udpReceiveBuffersCbVec[consumeIndex].resize(bytesTransferred);
        m_wholeBundleReadyCallback(m_udpReceiveBuffersCbVec[consumeIndex]);
        //if (m_udpReceiveBuffersCbVec[consumeIndex].size() != 0) {
        //}
        m_udpReceiveBuffersCbVec[consumeIndex].resize(M_MAX_UDP_PACKET_SIZE_BYTES); //restore for next udp read in case it was moved
        
        m_circularIndexBuffer.CommitRead();
    }

    LOG_INFO(subprocess) << "UdpBundleSink Circular buffer reader thread exiting";

}

void UdpBundleSink::DoUdpShutdown() {
    boost::asio::post(m_ioServiceRef, boost::bind(&UdpBundleSink::HandleSocketShutdown, this));
}

void UdpBundleSink::HandleSocketShutdown() {
    //final code to shut down tcp sockets
    if (m_udpSocket.is_open()) {
        try {
            LOG_INFO(subprocess) << "closing UdpBundleSink UDP socket..";
            m_udpSocket.close();
        }
        catch (const boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "UdpBundleSink::DoUdpShutdown: " << e.what();
        }
    }
    m_safeToDelete = true;
    if (m_notifyReadyToDeleteCallback) {
        m_notifyReadyToDeleteCallback();
    }
}

bool UdpBundleSink::ReadyToBeDeleted() {
    return m_safeToDelete.load(std::memory_order_acquire);
}

void UdpBundleSink::GetTelemetry(UdpInductConnectionTelemetry_t& telem) const {
    telem.m_connectionName = m_connectionNamePtr.load(std::memory_order_acquire); //always points to valid constant string
    telem.m_inputName = M_INPUT_NAME;
    telem.m_countCircularBufferOverruns = m_countCircularBufferOverruns.load(std::memory_order_acquire);
    telem.m_totalBundleBytesReceived = m_totalBundleBytesReceived.load(std::memory_order_acquire);
    telem.m_totalBundlesReceived = m_totalBundlesReceived.load(std::memory_order_acquire);
}
