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
#include "UdpBatchSender.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>
#include <memory>
#include <boost/make_unique.hpp>
#include <boost/endian/conversion.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

UdpBatchSender::UdpBatchSender() : m_udpSocketConnectedSenderOnly(m_ioService) {}

UdpBatchSender::~UdpBatchSender() {
    Stop();
}

bool UdpBatchSender::Init(const std::string& remoteHostname, const uint16_t remotePort) {

    if (m_ioServiceThreadPtr) {
        LOG_ERROR(subprocess) << "UdpBatchSender::Init: already initialized";
        return false;
    }
    m_ioService.reset();

    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    LOG_INFO(subprocess) << "UdpBatchSender resolving " << remoteHostname << ":" << remotePort;
    
    boost::asio::ip::udp::endpoint udpDestinationEndpoint;
    {
        boost::asio::ip::udp::resolver resolver(m_ioService);
        try {
            udpDestinationEndpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), remoteHostname, boost::lexical_cast<std::string>(remotePort), UDP_RESOLVER_FLAGS));
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "Error resolving in UdpBatchSender::Init: " << e.what() << "  code=" << e.code();
            return false;
        }
    }
    return Init(udpDestinationEndpoint);
}
bool UdpBatchSender::Init(const boost::asio::ip::udp::endpoint& udpDestinationEndpoint) {
#ifndef _WIN32
    if (sizeof(boost::asio::const_buffer) != sizeof(struct iovec)) {
        LOG_ERROR(subprocess) << "UdpBatchSender::Init: sizeof(boost::asio::const_buffer) != sizeof(struct iovec)";
        return false;
    }
#endif

    if (m_ioServiceThreadPtr) {
        LOG_ERROR(subprocess) << "UdpBatchSender::Init: already initialized";
        return false;
    }
    m_ioService.reset();

    m_udpDestinationEndpoint = udpDestinationEndpoint;
    try {
        m_udpSocketConnectedSenderOnly.open(boost::asio::ip::udp::v4());
        m_udpSocketConnectedSenderOnly.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)); //if udpPort is 0 then bind to random ephemeral port
    }
    catch (const boost::system::system_error& e) {
        LOG_ERROR(subprocess) << "Could not bind on random ephemeral UDP port";
        LOG_ERROR(subprocess) << "  Error: " << e.what();
        return false;
    }

    try {
        m_udpSocketConnectedSenderOnly.connect(m_udpDestinationEndpoint);
    }
    catch (const boost::system::system_error& e) {
        LOG_INFO(subprocess) << "Error connecting socket in UdpBatchSender::Init: " << e.what() << "  code=" << e.code();
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
# ifdef UDP_BATCH_SENDER_USE_OVERLAPPED
    // Overlapped operation used for TransmitPackets
    ZeroMemory(&m_sendOverlappedAutoReset, sizeof(m_sendOverlappedAutoReset));
    m_sendOverlappedAutoReset.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL); //auto-reset event object
    if (m_sendOverlappedAutoReset.hEvent == NULL) {
        printf("%s:%d - CreateEvent failed (%d)\n", __FILE__, __LINE__, GetLastError());
        return false;
    }
# endif
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
# ifdef UDP_BATCH_SENDER_USE_OVERLAPPED
        if (!CloseHandle(m_sendOverlappedAutoReset.hEvent)) {
            LOG_ERROR(subprocess) << "UdpBatchSender::Stop(), unable to close handle sendOverlapped";
        }
# endif
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
            LOG_INFO(subprocess) << "shutting down UdpBatchSender UDP socket..";
            m_udpSocketConnectedSenderOnly.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "UdpBatchSender::DoHandleSocketShutdown: " << e.what();
        }
        try {
            LOG_INFO(subprocess) << "closing UdpBatchSender UDP socket..";
            m_udpSocketConnectedSenderOnly.close();
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "UdpBatchSender::DoHandleSocketShutdown: " << e.what();
        }
    }
}

void UdpBatchSender::SetOnSentPacketsCallback(const OnSentPacketsCallback_t& callback) {
    m_onSentPacketsCallback = callback;
}

void UdpBatchSender::QueueSendPacketsOperation_ThreadSafe(
    std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
    std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
    std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec)
{
    boost::asio::post(m_ioService,
        boost::bind(&UdpBatchSender::PerformSendPacketsOperation, this,
            std::move(constBufferVecs), std::move(underlyingDataToDeleteOnSentCallbackVec), std::move(underlyingCsDataToDeleteOnSentCallbackVec)));
}

void UdpBatchSender::PerformSendPacketsOperation(
    std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
    std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
    std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec)
{

    m_transmitPacketsElementVec.resize(0); //reserved 100 elements in Init()
    

    for (std::size_t i = 0; i < constBufferVecs.size(); ++i) {
        std::vector<boost::asio::const_buffer>& currentPacketConstBuffers = constBufferVecs[i];
        const std::size_t currentPacketConstBuffersSize = currentPacketConstBuffers.size();
        if (currentPacketConstBuffersSize) {
#ifdef _WIN32
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
#else //not _WIN32
            /*
            the following two data structures are equivalent so a cast will work
            struct iovec {
                ptr_t iov_base; // Starting address
                size_t iov_len; // Length in bytes
            };
            class const_buffer
            {
            private:
                const void* data_;
                std::size_t size_;
            };
            */
            m_transmitPacketsElementVec.emplace_back();
            struct mmsghdr & mmsgHeader = m_transmitPacketsElementVec.back();
            /*
            struct msghdr {
                void    *   msg_name;   // Socket name
                int     msg_namelen;    // Length of name
                struct iovec* msg_iov;    // Data blocks
                __kernel_size_t msg_iovlen; // Number of blocks
                void* msg_control;    // Per protocol magic (eg BSD file descriptor passing)
                __kernel_size_t msg_controllen; // Length of cmsg list
                unsigned int    msg_flags;
            };
        
            // For recvmmsg/sendmmsg
            struct mmsghdr {
                struct msghdr msg_hdr;
                unsigned int  msg_len; //The msg_len field is used to return the number of
                                       //bytes sent from the message in msg_hdr (i.e., the same as the
                                       //return value from a single sendmsg(2) call).
            };
            */
            memset(&mmsgHeader, 0, sizeof(struct mmsghdr));
            mmsgHeader.msg_hdr.msg_iov = reinterpret_cast<struct iovec*>(currentPacketConstBuffers.data());
            mmsgHeader.msg_hdr.msg_iovlen = currentPacketConstBuffersSize;
                
#endif //#ifdef _WIN32
        }
        else {}
    }

    bool successfulSend = true;

    if (m_transmitPacketsElementVec.size()) { //there is data to send.. however if there is nothing to send then succeed with callback function and don't call OS routine
#ifdef _WIN32
        // Send the first burst of packets
        SOCKET nativeSocketHandle = m_udpSocketConnectedSenderOnly.native_handle();
        LPTRANSMIT_PACKETS_ELEMENT lpPacketArray = m_transmitPacketsElementVec.data();
        bool result = (*m_transmitPacketsFunctionPointer)(nativeSocketHandle,
            lpPacketArray,
            (DWORD)m_transmitPacketsElementVec.size(),
            0xFFFFFFFF, //TODO DETERMINE NUMBER OF F'S (7 OR 8).. Setting nSendSize to 0xFFFFFFF enables the caller to control the size and content of each send request,
                        // achieved by using the TP_ELEMENT_EOP flag in the TRANSMIT_PACKETS_ELEMENT array pointed to in the lpPacketArray parameter.
# ifdef UDP_BATCH_SENDER_USE_OVERLAPPED
            & m_sendOverlappedAutoReset,
# else
            NULL,
# endif
            TF_USE_DEFAULT_WORKER); //TF_USE_KERNEL_APC Directs Winsock to use kernel Asynchronous Procedure Calls (APCs) instead of worker threads to process
                                    // long TransmitPackets requests.
                                    // Long TransmitPackets requests are defined as requests that require more than a single read from the file or a cache;
                                    // the long request definition therefore depends on the size of the file and the specified length of the send packet.

        if (!result) {
            DWORD lastError = WSAGetLastError();
            if (lastError != ERROR_IO_PENDING) {
                printf("%s:%d - TransmitPackets failed (%d)\n", __FILE__, __LINE__, GetLastError());
                successfulSend = false;
            }
        }

# ifdef UDP_BATCH_SENDER_USE_OVERLAPPED
        DWORD waitResult = WaitForSingleObject(m_sendOverlappedAutoReset.hEvent, 2000); //2 second timeout
        if (waitResult == WAIT_OBJECT_0) { //success
            //successfulSend = true; //already initialized to true
        }
        else if (waitResult == WAIT_TIMEOUT) { //fail
            successfulSend = false;
        }
        else { //unknown error
            successfulSend = false;
        }
# endif //ifdef UDP_BATCH_SENDER_USE_OVERLAPPED

#else //not #ifdef _WIN32
        const int sockfd = m_udpSocketConnectedSenderOnly.native_handle();
        const unsigned int vlen = static_cast<unsigned int>(m_transmitPacketsElementVec.size());

        
        //A blocking sendmmsg() call (WHICH THIS IS) blocks until vlen messages have been
        //sent.  A nonblocking call sends as many messages as possible (up
        //to the limit specified by vlen) and returns immediately.
        const int retval = sendmmsg(sockfd, m_transmitPacketsElementVec.data(), vlen, 0);

        //On success, sendmmsg() returns the number of messages sent from
        //msgvec; if this is less than vlen, the caller can retry with a
        //further sendmmsg() call to send the remaining messages.
        //
        //On error, -1 is returned, and errno is set to indicate the error.
        if (retval == -1) {
            successfulSend = false;
            printf("%s:%d - sendmmsg failed with errno %d\n", __FILE__, __LINE__, errno);
            perror("sendmmsg()");
        }
        else if (retval != vlen) {
            //On return from sendmmsg(), the msg_len fields of successive
            //elements of msgvec are updated to contain the number of bytes
            //transmitted from the corresponding msg_hdr.  The return value of
            //the call indicates the number of elements of msgvec that have
            //been updated.
            successfulSend = false;
            printf("%s:%d - sendmmsg failed by only sending %d out of %d udp packets\n", __FILE__, __LINE__, retval, vlen);
        }

#endif //#ifdef _WIN32
    }

    if (m_onSentPacketsCallback) {
        m_onSentPacketsCallback(successfulSend, constBufferVecs, underlyingDataToDeleteOnSentCallbackVec, underlyingCsDataToDeleteOnSentCallbackVec);
    }
}

void UdpBatchSender::SetEndpointAndReconnect_ThreadSafe(const boost::asio::ip::udp::endpoint& remoteEndpoint) {
    boost::asio::post(m_ioService, boost::bind(&UdpBatchSender::SetEndpointAndReconnect, this, remoteEndpoint));
}
void UdpBatchSender::SetEndpointAndReconnect_ThreadSafe(const std::string& remoteHostname, const uint16_t remotePort) {
    boost::asio::post(m_ioService, boost::bind(&UdpBatchSender::SetEndpointAndReconnect, this, remoteHostname, remotePort));
}
bool UdpBatchSender::SetEndpointAndReconnect(const boost::asio::ip::udp::endpoint& remoteEndpoint) {
    m_udpDestinationEndpoint = remoteEndpoint;
    try {
        m_udpSocketConnectedSenderOnly.connect(m_udpDestinationEndpoint);
    }
    catch (const boost::system::system_error& e) {
        LOG_INFO(subprocess) << "Error connecting socket in UdpBatchSender::SetEndpointAndReconnect: " << e.what() << "  code=" << e.code();
        return false;
    }
    return true;
}
bool UdpBatchSender::SetEndpointAndReconnect(const std::string& remoteHostname, const uint16_t remotePort) {
    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    LOG_INFO(subprocess) << "UdpBatchSender resolving " << remoteHostname << ":" << remotePort;

    boost::asio::ip::udp::endpoint udpDestinationEndpoint;
    {
        boost::asio::ip::udp::resolver resolver(m_ioService);
        try {
            udpDestinationEndpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), remoteHostname, boost::lexical_cast<std::string>(remotePort), UDP_RESOLVER_FLAGS));
        }
        catch (const boost::system::system_error& e) {
            LOG_INFO(subprocess) << "Error resolving in UdpBatchSender::SetEndpointAndReconnect: " << e.what() << "  code=" << e.code();
            return false;
        }
    }
    return SetEndpointAndReconnect(udpDestinationEndpoint);
}
boost::asio::ip::udp::endpoint UdpBatchSender::GetCurrentUdpEndpoint() const {
    return m_udpDestinationEndpoint;
}
