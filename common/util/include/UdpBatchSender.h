/**
 * @file UdpBatchSender.h
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
 *
 * @section DESCRIPTION
 *
 * https://stackoverflow.com/questions/69729835/how-to-avoid-combining-multiple-buffers-into-one-udp-packet-with-function-call-w
 * https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/netds/Qos/Qos2/qossample.c
 * This UdpBatchSender class encapsulates the appropriate UDP functionality
 * to send multiple UDP packets in one system call in order to increase UDP throughput.
 * It also benefits in performance because the UDP socket must be "connected".
 */

#ifndef _UDP_BATCH_SENDER_H
#define _UDP_BATCH_SENDER_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "hdtn_util_export.h"
#include "LtpClientServiceDataToSend.h"
#include <memory>

class UdpBatchSender {
public:
    typedef boost::function<void(bool success, std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
        std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
        std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec)> OnSentPacketsCallback_t;
    HDTN_UTIL_EXPORT UdpBatchSender();
    HDTN_UTIL_EXPORT ~UdpBatchSender();
    HDTN_UTIL_EXPORT void Stop();
    HDTN_UTIL_EXPORT bool Init(const std::string& remoteHostname, const uint16_t remotePort);
    HDTN_UTIL_EXPORT bool Init(const boost::asio::ip::udp::endpoint & udpDestinationEndpoint);
    HDTN_UTIL_EXPORT boost::asio::ip::udp::endpoint GetCurrentUdpEndpoint() const;

    HDTN_UTIL_EXPORT void QueueSendPacketsOperation_ThreadSafe(std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
        std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
        std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec);
    HDTN_UTIL_EXPORT void SetOnSentPacketsCallback(const OnSentPacketsCallback_t& callback);
    HDTN_UTIL_EXPORT void SetEndpointAndReconnect_ThreadSafe(const boost::asio::ip::udp::endpoint& remoteEndpoint);
    HDTN_UTIL_EXPORT void SetEndpointAndReconnect_ThreadSafe(const std::string& remoteHostname, const uint16_t remotePort);

private:
    HDTN_UTIL_NO_EXPORT bool SetEndpointAndReconnect(const boost::asio::ip::udp::endpoint& remoteEndpoint);
    HDTN_UTIL_NO_EXPORT bool SetEndpointAndReconnect(const std::string& remoteHostname, const uint16_t remotePort);
    HDTN_UTIL_NO_EXPORT void PerformSendPacketsOperation(
        std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
        std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
        std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec);
    HDTN_UTIL_NO_EXPORT void DoUdpShutdown();
    HDTN_UTIL_NO_EXPORT void DoHandleSocketShutdown();
private:

    boost::asio::io_service m_ioService;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    boost::asio::ip::udp::socket m_udpSocketConnectedSenderOnly;
    boost::asio::ip::udp::endpoint m_udpDestinationEndpoint;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
#ifdef _WIN32
//# define UDP_BATCH_SENDER_USE_OVERLAPPED 1
    LPFN_TRANSMITPACKETS m_transmitPacketsFunctionPointer;
# ifdef UDP_BATCH_SENDER_USE_OVERLAPPED
    WSAOVERLAPPED m_sendOverlappedAutoReset;
# endif
    std::vector<TRANSMIT_PACKETS_ELEMENT> m_transmitPacketsElementVec;
#else //not #ifdef _WIN32
    std::vector<struct mmsghdr> m_transmitPacketsElementVec;
#endif
    
    OnSentPacketsCallback_t m_onSentPacketsCallback;
};



#endif //_UDP_BATCH_SENDER_H
