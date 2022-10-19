/**
 * @file UdpDelaySim.h
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
 * This UdpDelaySim class is a proxy server which delays incoming udp packets
 * by a specified millisecond delay before forwarding those packets on to
 * the specified remote destination.
 * The direction is one way, so create two separate instances/processes of UdpDelaySim
 * in order to achieve bidirectional communication such as LTP.
 */

#ifndef _UDP_DELAY_SIM_H
#define _UDP_DELAY_SIM_H 1

#include <stdint.h>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/date_time.hpp>
#include <list>
#include "udp_delay_sim_lib_export.h"



class UdpDelaySim {
private:
    UdpDelaySim();
public:
    typedef boost::function<bool(const std::vector<uint8_t> & udpPacketReceived, std::size_t bytesTransferred)> UdpDropSimulatorFunction_t;
    UDP_DELAY_SIM_LIB_EXPORT UdpDelaySim(uint16_t myBoundUdpPort,
        const std::string & remoteHostnameToForwardPacketsTo,
        const std::string & remotePortToForwardPacketsTo,
        const unsigned int numCircularBufferVectors,
        const unsigned int maxUdpPacketSizeBytes,
        const boost::posix_time::time_duration & sendDelay,
        const bool autoStart);
    UDP_DELAY_SIM_LIB_EXPORT ~UdpDelaySim();
    UDP_DELAY_SIM_LIB_EXPORT void Stop();
    UDP_DELAY_SIM_LIB_EXPORT bool StartIfNotAlreadyRunning();
    UDP_DELAY_SIM_LIB_EXPORT void DoUdpShutdown();
    UDP_DELAY_SIM_LIB_EXPORT void SetUdpDropSimulatorFunction_ThreadSafe(const UdpDropSimulatorFunction_t & udpDropSimulatorFunction);
    UDP_DELAY_SIM_LIB_EXPORT void QueuePacketForDelayedSend_NotThreadSafe(std::vector<uint8_t> & udpPacketToSwapIn, std::size_t bytesTransferred);
private:
    UDP_DELAY_SIM_LIB_NO_EXPORT void SetUdpDropSimulatorFunction(const UdpDropSimulatorFunction_t & udpDropSimulatorFunction);
    UDP_DELAY_SIM_LIB_NO_EXPORT void OnResolve(const boost::system::error_code & ec, boost::asio::ip::udp::resolver::results_type results);
    UDP_DELAY_SIM_LIB_NO_EXPORT void StartUdpReceive();
    UDP_DELAY_SIM_LIB_NO_EXPORT void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred);
    UDP_DELAY_SIM_LIB_NO_EXPORT void HandleUdpSend(const boost::system::error_code& error, std::size_t bytes_transferred);

    UDP_DELAY_SIM_LIB_NO_EXPORT void TryRestartSendDelayTimer();
    UDP_DELAY_SIM_LIB_NO_EXPORT void OnSendDelay_TimerExpired(const boost::system::error_code& e, const unsigned int consumeIndex);

    UDP_DELAY_SIM_LIB_NO_EXPORT void TransferRate_TimerExpired(const boost::system::error_code& e);
public:
    

private:

    boost::asio::io_service m_ioService;
    boost::asio::ip::udp::resolver m_resolver;
    boost::asio::deadline_timer m_udpPacketSendDelayTimer;
    boost::asio::deadline_timer m_timerTransferRateStats;
    boost::asio::ip::udp::socket m_udpSocket;

    boost::posix_time::ptime m_lastPtime;
    
    const uint16_t M_MY_BOUND_UDP_PORT;
    const std::string M_REMOTE_HOSTNAME_TO_FORWARD_PACKETS_TO;
    const std::string M_REMOTE_PORT_TO_FORWARD_PACKETS_TO;
    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    const unsigned int M_MAX_UDP_PACKET_SIZE_BYTES;
    const boost::posix_time::time_duration M_SEND_DELAY;
    std::vector<uint8_t> m_udpReceiveBuffer;
    boost::asio::ip::udp::endpoint m_remoteEndpointReceived;
    boost::asio::ip::udp::endpoint m_udpDestinationEndpoint;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::vector<uint8_t> > m_udpReceiveBuffersCbVec;
    std::vector<uint64_t> m_udpReceiveBytesTransferredCbVec;
    std::vector<boost::posix_time::ptime> m_expiriesCbVec;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    bool m_printedCbTooSmallNotice;
    bool m_sendDelayTimerIsRunning;
    volatile bool m_setUdpDropSimulatorFunctionInProgress;
    boost::condition_variable m_cvSetUdpDropSimulatorFunction;
    UdpDropSimulatorFunction_t m_udpDropSimulatorFunction;

    //stats
    uint64_t m_lastTotalUdpPacketsReceived;
    uint64_t m_lastTotalUdpBytesReceived;
    uint64_t m_lastTotalUdpPacketsSent;
    uint64_t m_lastTotalUdpBytesSent;
public:
    uint64_t m_countCircularBufferOverruns;
    uint64_t m_countMaxCircularBufferSize;
    uint64_t m_countTotalUdpPacketsReceived;
    uint64_t m_countTotalUdpBytesReceived;
    uint64_t m_countTotalUdpPacketsSent;
    uint64_t m_countTotalUdpBytesSent;
};

#endif  //_UDP_DELAY_SIM_H
