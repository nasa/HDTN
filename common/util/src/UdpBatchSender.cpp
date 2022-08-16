/**
 * @file UdpBatchSender.cpp
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
 */

#include <string>
#include <iostream>
#include "UdpBatchSender.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include <boost/endian/conversion.hpp>

UdpBatchSender::UdpBatchSender() : m_udpSocketConnectedSenderOnly(m_ioService) {}

UdpBatchSender::~UdpBatchSender() {
    Stop();
}

bool UdpBatchSender::Init(const std::string& remoteHostname, const std::string& remotePort) {

    if (m_ioServiceThreadPtr) {
        std::cout << "Error in UdpBatchSender::Init: already initialized\n";
        return false;
    }
    m_ioService.reset();

    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    std::cout << "UdpBatchSender resolving " << remoteHostname << ":" << remotePort << std::endl;
    boost::asio::ip::udp::resolver resolver(m_ioService);
    try {
        m_udpDestinationEndpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), remoteHostname, boost::lexical_cast<std::string>(remotePort), UDP_RESOLVER_FLAGS));
    }
    catch (const boost::system::system_error& e) {
        std::cout << "Error resolving in UdpBatchSender::Init: " << e.what() << "  code=" << e.code() << std::endl;
        return false;
    }

    try {
        m_udpSocketConnectedSenderOnly.open(boost::asio::ip::udp::v4());
        m_udpSocketConnectedSenderOnly.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)); //if udpPort is 0 then bind to random ephemeral port
    }
    catch (const boost::system::system_error& e) {
        std::cerr << "Could not bind on random ephemeral UDP port\n";
        std::cerr << "  Error: " << e.what() << std::endl;
        return false;
    }

    try {
        m_udpSocketConnectedSenderOnly.connect(m_udpDestinationEndpoint);
    }
    catch (const boost::system::system_error& e) {
        std::cout << "Error connecting socket in UdpBatchSender::Init: " << e.what() << "  code=" << e.code() << std::endl;
        return false;
    }

#ifdef _WIN32
    // Query the function pointer for the TransmitPacket function
    SOCKET nativeSocketHandle = m_udpSocketConnectedSenderOnly.native_handle();
    GUID TransmitPacketsGuid = WSAID_TRANSMITPACKETS;
    DWORD bytesReturned;
    int returnValue = WSAIoctl(nativeSocketHandle,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &TransmitPacketsGuid,
        sizeof(GUID),
        &m_transmitPacketsFunctionPointer,
        sizeof(PVOID),
        &bytesReturned,
        NULL,
        NULL);

    if (returnValue == SOCKET_ERROR) {
        printf("%s:%d - WSAIoctl failed (%d)\n", __FILE__, __LINE__, WSAGetLastError());
        return false;
    }

    // Overlapped operation used for TransmitPackets
    ZeroMemory(&m_sendOverlappedAutoReset, sizeof(m_sendOverlappedAutoReset));
    m_sendOverlappedAutoReset.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL); //auto-reset event object
    if (m_sendOverlappedAutoReset.hEvent == NULL) {
        printf("%s:%d - CreateEvent failed (%d)\n", __FILE__, __LINE__, GetLastError());
        return false;
    }

    m_transmitPacketsElementVec.reserve(100);
#endif

    m_workPtr = boost::make_unique<boost::asio::io_service::work>(m_ioService);
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

    return true;
}

void UdpBatchSender::Stop() {
    if (m_ioServiceThreadPtr) {
        DoUdpShutdown();
        while (m_udpSocketConnectedSenderOnly.is_open()) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
        }

        //This function does not block, but instead simply signals the io_service to stop
        //All invocations of its run() or run_one() member functions should return as soon as possible.
        //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
        m_workPtr.reset();
        if (!m_ioService.stopped()) {
            m_ioService.stop(); //ioservice requires stopping before join because of the m_work object
        }
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it

#ifdef _WIN32
        if (!CloseHandle(m_sendOverlappedAutoReset.hEvent)) {
            std::cout << "error in UdpBatchSender::Stop(), unable to close handle sendOverlapped\n";
        }
#endif
    }

}

void UdpBatchSender::DoUdpShutdown() {
    boost::asio::post(m_ioService, boost::bind(&UdpBatchSender::DoHandleSocketShutdown, this));
}

void UdpBatchSender::DoHandleSocketShutdown() {
    //final code to shut down tcp sockets
    if (m_udpSocketConnectedSenderOnly.is_open()) {
        try {
            std::cout << "shutting down UdpBatchSender UDP socket.." << std::endl;
            m_udpSocketConnectedSenderOnly.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        }
        catch (const boost::system::system_error& e) {
            std::cerr << "error in UdpBatchSender::DoHandleSocketShutdown: " << e.what() << std::endl;
        }
        try {
            std::cout << "closing UdpBatchSender UDP socket.." << std::endl;
            m_udpSocketConnectedSenderOnly.close();
        }
        catch (const boost::system::system_error& e) {
            std::cerr << "error in UdpBatchSender::DoHandleSocketShutdown: " << e.what() << std::endl;
        }
    }
}

void UdpBatchSender::QueueSendPacketsOperation_ThreadSafe(
    std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
    boost::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback)
{
    /*
    //called by LtpEngine Thread
    ++m_countAsyncSendCalls;
    if (m_udpDropSimulatorFunction && m_udpDropSimulatorFunction(*((uint8_t*)constBufferVec[0].data()))) {
        boost::asio::post(m_ioServiceUdpRef, boost::bind(&LtpUdpEngine::HandleUdpSend, this, underlyingDataToDeleteOnSentCallback, boost::system::error_code(), 0));
    }
    else {
        m_udpSocketRef.async_send_to(constBufferVec, m_remoteEndpoint,
            boost::bind(&LtpUdpEngine::HandleUdpSend, this, std::move(underlyingDataToDeleteOnSentCallback),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }*/
    boost::asio::post(m_ioService,
        boost::bind(&UdpBatchSender::PerformSendPacketsOperation, this,
            std::move(constBufferVecs), std::move(underlyingDataToDeleteOnSentCallback)));
}

void UdpBatchSender::PerformSendPacketsOperation(
    std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
    boost::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback)
{
#ifdef _WIN32
    m_transmitPacketsElementVec.resize(0); //reserved 100 elements in Init()
    

    for (std::size_t i = 0; i < constBufferVecs.size(); ++i) {
        std::vector<boost::asio::const_buffer>& currentPacketConstBuffers = constBufferVecs[i];
        const std::size_t currentPacketConstBuffersSize = currentPacketConstBuffers.size();
        if (currentPacketConstBuffersSize) {
            std::size_t j = 0;
            while(true) {
                m_transmitPacketsElementVec.emplace_back();
                TRANSMIT_PACKETS_ELEMENT& tpe = m_transmitPacketsElementVec.back();
                boost::asio::const_buffer& constBuf = currentPacketConstBuffers[j];
                tpe.dwElFlags = TP_ELEMENT_MEMORY;
                tpe.pBuffer = (PVOID)constBuf.data();
                tpe.cLength = (ULONG)constBuf.size();
                ++j;
                if (j == currentPacketConstBuffersSize) {
                    break;
                }
            }
            m_transmitPacketsElementVec.back().dwElFlags |= TP_ELEMENT_EOP;
        }
    }

    

    // Send the first burst of packets
    SOCKET nativeSocketHandle = m_udpSocketConnectedSenderOnly.native_handle();
    LPTRANSMIT_PACKETS_ELEMENT lpPacketArray = m_transmitPacketsElementVec.data();
    bool result = (*m_transmitPacketsFunctionPointer)(nativeSocketHandle,
        lpPacketArray,
        (DWORD)m_transmitPacketsElementVec.size(),
        0xFFFFFFFF, //TODO DETERMINE NUMBER OF F'S (7 OR 8).. Setting nSendSize to 0xFFFFFFF enables the caller to control the size and content of each send request,
                    // achieved by using the TP_ELEMENT_EOP flag in the TRANSMIT_PACKETS_ELEMENT array pointed to in the lpPacketArray parameter.
        &m_sendOverlappedAutoReset,
        TF_USE_KERNEL_APC); //Directs Winsock to use kernel Asynchronous Procedure Calls (APCs) instead of worker threads to process long TransmitPackets requests.
                            // Long TransmitPackets requests are defined as requests that require more than a single read from the file or a cache;
                            // the long request definition therefore depends on the size of the file and the specified length of the send packet.

    if (!result) {
        DWORD lastError = WSAGetLastError();
        if (lastError != ERROR_IO_PENDING) {
            printf("%s:%d - TransmitPackets failed (%d)\n", __FILE__, __LINE__, GetLastError());
            return;
        }
    }

    DWORD waitResult = WaitForSingleObject(m_sendOverlappedAutoReset.hEvent, 2000); //2 second timeout
    if (waitResult == WAIT_OBJECT_0) { //success

    }
    else if (waitResult == WAIT_TIMEOUT) { //fail

    }
    else { //unknown error

    }

#endif
}

