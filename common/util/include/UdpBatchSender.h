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
 */

#ifndef _UDP_BATCH_SENDER_H
#define _UDP_BATCH_SENDER_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "hdtn_util_export.h"


class UdpBatchSender {
public:
    typedef boost::function<void()> OnSuccessfulAckCallback_t;
    HDTN_UTIL_EXPORT UdpBatchSender();
    HDTN_UTIL_EXPORT ~UdpBatchSender();
    HDTN_UTIL_EXPORT void Stop();
    HDTN_UTIL_EXPORT bool Init(const std::string& remoteHostname, const std::string& remotePort);

    HDTN_UTIL_EXPORT void QueueSendPacketsOperation_ThreadSafe(std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
        boost::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback);


private:
    HDTN_UTIL_NO_EXPORT void PerformSendPacketsOperation(
        std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
        boost::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback);
    HDTN_UTIL_NO_EXPORT void DoUdpShutdown();
    HDTN_UTIL_NO_EXPORT void DoHandleSocketShutdown();
private:

    boost::asio::io_service m_ioService;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    boost::asio::ip::udp::socket m_udpSocketConnectedSenderOnly;
    boost::asio::ip::udp::endpoint m_udpDestinationEndpoint;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
#ifdef _WIN32
    LPFN_TRANSMITPACKETS m_transmitPacketsFunctionPointer;
    WSAOVERLAPPED m_sendOverlappedAutoReset;
    std::vector<TRANSMIT_PACKETS_ELEMENT> m_transmitPacketsElementVec;
#endif
    
    //OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;
};



#endif //_UDP_BATCH_SENDER_H
