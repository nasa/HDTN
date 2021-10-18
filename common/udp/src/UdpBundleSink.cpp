#include <boost/bind/bind.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "UdpBundleSink.h"
#include <boost/endian/conversion.hpp>
#include <boost/make_unique.hpp>

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
    m_countCircularBufferOverruns(0),
    m_printedCbTooSmallNotice(false)
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
        std::cerr << "Could not bind on UDP port " << udpPort << std::endl;
        std::cerr << "  Error: " << e.what() << std::endl;

        m_running = false;
        return;
    }
    printf("UdpBundleSink bound successfully on UDP port %d ...", udpPort);
    StartUdpReceive(); //call before creating io_service thread so that it has "work"
}

UdpBundleSink::~UdpBundleSink() {

    if (!m_safeToDelete) {
        DoUdpShutdown();
        while (!m_safeToDelete) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
        }
    }
    
    
    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr.reset(); //delete it
    }
    std::cout << "UdpBundleSink m_countCircularBufferOverruns: " << m_countCircularBufferOverruns << std::endl;
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
    //std::cout << "1" << std::endl;
    if (!error) {
        const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == UINT32_MAX) {
            ++m_countCircularBufferOverruns;
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                std::cout << "notice in LtpUdpEngine::StartUdpReceive(): buffers full.. you might want to increase the circular buffer size! This UDP packet will be dropped!" << std::endl;
            }
        }
        else {
            m_udpReceiveBuffer.swap(m_udpReceiveBuffersCbVec[writeIndex]);
            m_udpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
            m_remoteEndpointsCbVec[writeIndex] = std::move(m_remoteEndpoint);
            m_circularIndexBuffer.CommitWrite(); //write complete at this point
            m_conditionVariableCb.notify_one();
        }
        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "critical error in UdpBundleSink::HandleUdpReceive(): " << error.message() << std::endl;
        DoUdpShutdown();
    }
}




void UdpBundleSink::PopCbThreadFunc() {

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (m_circularIndexBuffer.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile

        if (consumeIndex == UINT32_MAX) { //if empty
            m_conditionVariableCb.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        //m_wholeBundleReadyCallback(m_udpReceiveBuffersCbVec[consumeIndex], m_udpReceiveBytesTransferredCbVec[consumeIndex]);
        m_udpReceiveBuffersCbVec[consumeIndex].resize(m_udpReceiveBytesTransferredCbVec[consumeIndex]);
        m_wholeBundleReadyCallback(m_udpReceiveBuffersCbVec[consumeIndex]);
        //if (m_udpReceiveBuffersCbVec[consumeIndex].size() != 0) {
        //    std::cerr << "error in UdpBundleSink::PopCbThreadFunc(): udp data was not moved" << std::endl;
        //}
        m_udpReceiveBuffersCbVec[consumeIndex].resize(M_MAX_UDP_PACKET_SIZE_BYTES); //restore for next udp read in case it was moved
        
        m_circularIndexBuffer.CommitRead();
    }

    std::cout << "UdpBundleSink Circular buffer reader thread exiting\n";

}

void UdpBundleSink::DoUdpShutdown() {
    boost::asio::post(m_ioServiceRef, boost::bind(&UdpBundleSink::HandleSocketShutdown, this));
}

void UdpBundleSink::HandleSocketShutdown() {
    //final code to shut down tcp sockets
    if (m_udpSocket.is_open()) {
        try {
            std::cout << "closing UdpBundleSink UDP socket.." << std::endl;
            m_udpSocket.close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in UdpBundleSink::DoUdpShutdown: " << e.what() << std::endl;
        }
    }
    m_safeToDelete = true;
    if (m_notifyReadyToDeleteCallback) {
        m_notifyReadyToDeleteCallback();
    }
}

bool UdpBundleSink::ReadyToBeDeleted() {
    return m_safeToDelete;
}
