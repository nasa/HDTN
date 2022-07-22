/**
 * @file StcpBundleSource.h
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
 * This StcpBundleSource class encapsulates the appropriate "DTN simple TCP convergence layer (STCP)" functionality
 * to send a pipeline of bundles (or any other user defined data) over an STCP link
 * and calls the user defined function OnSuccessfulAckCallback_t when the session closes, meaning
 * a bundle is fully sent (i.e. the OS TCP protocol notified that the byte stream was delivered).
 * This class is implemented based on the ION.pdf V4.0.1 sections STCPCLI and STCPCLO.
 */

#ifndef _STCP_BUNDLE_SOURCE_H
#define _STCP_BUNDLE_SOURCE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <queue>
#include "TcpAsyncSender.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "Telemetry.h"
#include "stcp_lib_export.h"

class StcpBundleSource {
private:
    StcpBundleSource();
public:
    typedef boost::function<void()> OnSuccessfulAckCallback_t;
    STCP_LIB_EXPORT StcpBundleSource(const uint16_t desiredKeeAliveIntervalSeconds, const unsigned int maxUnacked = 100);

    STCP_LIB_EXPORT ~StcpBundleSource();
    STCP_LIB_EXPORT void Stop();
    STCP_LIB_EXPORT bool Forward(const uint8_t* bundleData, const std::size_t size);
    STCP_LIB_EXPORT bool Forward(zmq::message_t & dataZmq);
    STCP_LIB_EXPORT bool Forward(std::vector<uint8_t> & dataVec);
    STCP_LIB_EXPORT std::size_t GetTotalDataSegmentsAcked();
    STCP_LIB_EXPORT std::size_t GetTotalDataSegmentsSent();
    STCP_LIB_EXPORT std::size_t GetTotalDataSegmentsUnacked();
    STCP_LIB_EXPORT std::size_t GetTotalBundleBytesAcked();
    STCP_LIB_EXPORT std::size_t GetTotalBundleBytesSent();
    STCP_LIB_EXPORT std::size_t GetTotalBundleBytesUnacked();
    STCP_LIB_EXPORT void Connect(const std::string & hostname, const std::string & port);
    STCP_LIB_EXPORT bool ReadyToForward();
    STCP_LIB_EXPORT void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
private:
    STCP_LIB_NO_EXPORT static void GenerateDataUnit(std::vector<uint8_t> & dataUnit, const uint8_t * contents, uint32_t sizeContents);
    STCP_LIB_NO_EXPORT static void GenerateDataUnitHeaderOnly(std::vector<uint8_t> & dataUnit, uint32_t sizeContents);
    STCP_LIB_NO_EXPORT void OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results);
    STCP_LIB_NO_EXPORT void OnConnect(const boost::system::error_code & ec);
    STCP_LIB_NO_EXPORT void OnReconnectAfterOnConnectError_TimerExpired(const boost::system::error_code& e);
    STCP_LIB_NO_EXPORT void HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);
    STCP_LIB_NO_EXPORT void HandleTcpSendKeepAlive(const boost::system::error_code& error, std::size_t bytes_transferred);
    STCP_LIB_NO_EXPORT void StartTcpReceive();
    STCP_LIB_NO_EXPORT void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred);

    STCP_LIB_NO_EXPORT void OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);
    STCP_LIB_NO_EXPORT void DoStcpShutdown(unsigned int reconnectionDelaySecondsIfNotZero);
    STCP_LIB_NO_EXPORT void DoHandleSocketShutdown(unsigned int reconnectionDelaySecondsIfNotZero);
    STCP_LIB_NO_EXPORT void OnNeedToReconnectAfterShutdown_TimerExpired(const boost::system::error_code& e);
    
    std::unique_ptr<TcpAsyncSender> m_tcpAsyncSenderPtr;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_handleTcpSendCallback;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_handleTcpSendKeepAliveCallback;


    
    boost::asio::io_service m_ioService;
    boost::asio::io_service::work m_work;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_needToSendKeepAliveMessageTimer;
    boost::asio::deadline_timer m_reconnectAfterShutdownTimer;
    boost::asio::deadline_timer m_reconnectAfterOnConnectErrorTimer;
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    boost::asio::ip::tcp::resolver::results_type m_resolverResults;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    boost::condition_variable m_localConditionVariableAckReceived;

    const uint16_t M_KEEP_ALIVE_INTERVAL_SECONDS;
    const unsigned int MAX_UNACKED;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_bytesToAckByTcpSendCallbackCb;
    std::vector<uint32_t> m_bytesToAckByTcpSendCallbackCbVec;
    OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;
    volatile bool m_readyToForward;
    volatile bool m_stcpShutdownComplete;
    volatile bool m_dataServedAsKeepAlive;
    volatile bool m_useLocalConditionVariableAckReceived;

    uint8_t m_tcpReadSomeBuffer[10];

public:
    //stcp stats
    StcpOutductTelemetry_t m_stcpOutductTelemetry;
};



#endif //_STCP_BUNDLE_SOURCE_H
