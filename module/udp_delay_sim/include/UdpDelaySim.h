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
    /**
     * @typedef Type of predicate to be invoked during packet drop simulation.
     * @return True if the packet should be dropped, or False otherwise.
     */
    typedef boost::function<bool(const std::vector<uint8_t> & udpPacketReceived, std::size_t bytesTransferred)> UdpDropSimulatorFunction_t;
    
    /**
     * Bind I/O components to m_ioService.
     * Preallocate transmission buffers.
     * If autoStart is set to True, call UdpDelaySim::StartIfNotAlreadyRunning() to start the server.
     * @param myBoundUdpPort The port to bind the UDP server on.
     * @param remoteHostnameToForwardPacketsTo The forwarding destination host.
     * @param remotePortToForwardPacketsTo The forwarding destination port.
     * @param numCircularBufferVectors The size of the transmission buffers.
     * @param maxUdpPacketSizeBytes The maximum size of a UDP packet in bytes.
     * @param sendDelay The delay duration for packet forwarding.
     * @param autoStart Whether the server should start automatically on construction.
     */
    UDP_DELAY_SIM_LIB_EXPORT UdpDelaySim(uint16_t myBoundUdpPort,
        const std::string & remoteHostnameToForwardPacketsTo,
        const std::string & remotePortToForwardPacketsTo,
        const unsigned int numCircularBufferVectors,
        const unsigned int maxUdpPacketSizeBytes,
        const boost::posix_time::time_duration & sendDelay,
        uint64_t lossOfSignalStartMs,
        uint64_t lossOfSignalDurationMs,
        const bool autoStart);
    
    /// Call UdpDelaySim::Stop() to release managed resources
    UDP_DELAY_SIM_LIB_EXPORT ~UdpDelaySim();
    
    /** Perform server shutdown.
     *
     * Calls UdpDelaySim::DoUdpShutdown() to release UDP resources, then releases all underlying I/O resources.
     * @post The object is ready to be reused after the next successful call to UdpDelaySim::StartIfNotAlreadyRunning().
     */
    UDP_DELAY_SIM_LIB_EXPORT void Stop();
    
    /** Start the UDP server and initiate resolving of the forwarding destination endpoint.
     *
     * If the server is already running, returns immediately.
     * Starts the stat tracking interval loop, then starts the server.
     * Initiates an asynchronous resolve operation to resolve the forwarding destination endpoint with UdpDelaySim::OnResolve() as a completion handler.
     * @return True if the server could be started successfully or is already running, or False otherwise.
     * @post If the server was not already running, the server will NOT be listening for packets yet.
     */
    UDP_DELAY_SIM_LIB_EXPORT bool StartIfNotAlreadyRunning();
    
    /** Stop timers and close listening socket.
     *
     * Stops transmission and stat tracking timers, then closes the listening socket.
     * @post The UDP resources are ready to be reused, but the server will remain in the running state until the next call to UdpDelaySim::Stop().
     */
    UDP_DELAY_SIM_LIB_EXPORT void DoUdpShutdown();
    
    /** Initiate a request to set the udpDropSimulator function (thread-safe).
     *
     * Initiates an asynchronous request to UdpDelaySim::SetUdpDropSimulatorFunction().
     * The calling thread blocks until the request is resolved.
     * @param udpDropSimulatorFunction The function to set.
     */
    UDP_DELAY_SIM_LIB_EXPORT void SetUdpDropSimulatorFunction_ThreadSafe(const UdpDropSimulatorFunction_t & udpDropSimulatorFunction);
    
    /** Queue packet for delayed transmission.
     *
     * Timestamps the packet for transmission clearance using the configured transmission delay duration, then places the packet in the transmission queue.
     * udpPacketToSwapIn holds the entire receive buffer with a size of M_MAX_UDP_PACKET_SIZE_BYTES, only the first bytesTransferred bytes belong
     * in the current packet and are therefore safe to read, any bytes that follow may contain partial data from previously processed packets
     * and should be treated as garbage values.
     * @param udpPacketToSwapIn The packet to transmit.
     * @param bytesTransferred The size of the packet.
     * @post The argument to udpPacketToSwapIn is swapped with a potentially dirty buffer of an already-processed packet and is ready to
     * be reused as the receive buffer for the next read operation (see above how to handle dirty buffers).
     */
    UDP_DELAY_SIM_LIB_EXPORT void QueuePacketForDelayedSend_NotThreadSafe(std::vector<uint8_t> & udpPacketToSwapIn, std::size_t bytesTransferred);
private:
    /** Set the udpDropSimulator function.
     *
     * @param udpDropSimulatorFunction The function to set.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void SetUdpDropSimulatorFunction(const UdpDropSimulatorFunction_t & udpDropSimulatorFunction);
    
    /** Cache resolved forwarding destination UDP endpoint and start the receive loop for the packet origin.
     *
     * If an error occurred during the resolve operation, returns immediately letting m_ioService run out of work and leaving the server in an idle state, to retry
     * a server restart would be necessary, achieved by a call to UdpDelaySim::Stop() followed by a successful call to UdpDelaySim::StartIfNotAlreadyRunning().
     * Else, calls UdpDelaySim::StartUdpReceive() to start the receive loop.
     * @param ec The error code.
     * @param results The resolved UDP endpoint.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void OnResolve(const boost::system::error_code & ec, boost::asio::ip::udp::resolver::results_type results);
    
    /** Start the receive loop for the packet origin.
     *
     * Initiates an asynchronous receive operation with UdpDelaySim::HandleUdpReceive() as a completion handler.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void StartUdpReceive();
    
    /** Handle a UDP receive operation.
     *
     * If a fatal error occurred during the receive operation, calls UdpDelaySim::DoUdpShutdown().
     * If a packet is not to be dropped by the drop simulation utility (if/when active), queues packet for delayed transmission.
     * On successful processing, initiates an asynchronous receive operation with itself as a completion handler to achieve a receive loop.
     * @param error The error code.
     * @param bytesTransferred The number of bytes received.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred);
    
    /** Handle a UDP send operation.
     *
     * If an error occurred during the send operation, calls UdpDelaySim::DoUdpShutdown().
     * Else, calls UdpDelaySim::TryRestartSendDelayTimer() to queue the next packet for transmission.
     * @param error The error code.
     * @param bytes_transferred The number of bytes sent.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void HandleUdpSend(const boost::system::error_code& error, std::size_t bytes_transferred);

    /** Start the delay timer for the next packet in the transmission queue.
     *
     * If the timer is already running or there are no packets available in the transmission queue, returns immediately.
     * Else, starts the transmission delay timer asynchronously for the next packet in the transmission queue with OnSendDelay_TimerExpired() as a completion handler.
     * The timer runs for the remaining delay duration (if any left) from the time the packet was enqueued for transmission.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void TryRestartSendDelayTimer();
    
    /** Handle transmission delay timer expiry.
     *
     * If the expiry occurred due to the timer being manually cancelled, returns immediately.
     * Else, initiates an asynchronous send operation to forward the packet associated with consumeIndex to the forwarding destination,
     * with UdpDelaySim::HandleUdpSend() as a completion handler.
     * @param e The error code.
     * @param consumeIndex The index of the associated packet.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void OnSendDelay_TimerExpired(const boost::system::error_code& e, const unsigned int consumeIndex);
    
    /** Handle stat tracking timer expiry.
     *
     * If the expiry occurred due to the timer being manually cancelled, returns immediately.
     * Else, updates tracked stats and starts the stat tracking timer asynchronously with itself as a completion handler to achieve a stat tracking loop.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void TransferRate_TimerExpired(const boost::system::error_code& e);

    /** Handle LOS simulation.
     *
     * If the expiry occurred due to the timer being manually cancelled, returns immediately.
     * Else, updates boolean m_isStateLossOfSignal to determine if packets are being dropped to simulate an LOS.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void LossOfSignal_TimerExpired(const boost::system::error_code& e,
        bool isStartOfLos);
public:
    

private:

    /// I/O execution context
    boost::asio::io_service m_ioService;
    /// UDP endpoint resolver
    boost::asio::ip::udp::resolver m_resolver;
    /// Forwarding delay timer, on expiry forward kept packet
    boost::asio::deadline_timer m_udpPacketSendDelayTimer;
    /// Stat tracking timer, on expiry update stats and restart interval
    boost::asio::deadline_timer m_timerTransferRateStats;
    /// Loss of signal timer, sets bool m_isStateLossOfSignal
    boost::asio::deadline_timer m_timerLossOfSignal;
    /// UDP server socket
    boost::asio::ip::udp::socket m_udpSocket;

    /// Time point, used by the stat tracking mechanism to calculate delta time
    boost::posix_time::ptime m_lastPtime;
    
    /// Our UDP server port, if the port number 0 the server socket is bound to a random ephemeral port
    const uint16_t M_MY_BOUND_UDP_PORT;
    /// Forwarding destination host
    const std::string M_REMOTE_HOSTNAME_TO_FORWARD_PACKETS_TO;
    /// Forwarding destination port
    const std::string M_REMOTE_PORT_TO_FORWARD_PACKETS_TO;
    /// Number of transmission buffers
    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    /// Maximum size of a UDP packet in bytes
    const unsigned int M_MAX_UDP_PACKET_SIZE_BYTES;
    /// Forwarding delay duration
    const boost::posix_time::time_duration M_SEND_DELAY;
    /// Packet receive buffer
    std::vector<uint8_t> m_udpReceiveBuffer;
    /// Packet origin UDP endpoint
    boost::asio::ip::udp::endpoint m_remoteEndpointReceived;
    /// Forwarding destination UDP endpoint
    boost::asio::ip::udp::endpoint m_udpDestinationEndpoint;
    /// Circular index buffer, used to index the circular vector holding packet metadata necessary for transmission
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    /// Circular vector of receive buffers, each receive buffer holds the entire m_udpReceiveBuffer, NOT just the bytes of the received packet
    std::vector<std::vector<uint8_t> > m_udpReceiveBuffersCbVec;
    /// Circular vector of packet size per receive buffer, the actual size of the received packet
    std::vector<uint64_t> m_udpReceiveBytesTransferredCbVec;
    /// Circular vector of expiry time point per packet, the absolute time when the packet receives clearance for transmission
    std::vector<boost::posix_time::ptime> m_expiriesCbVec;
    /// Thread that invokes m_ioService.run()
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    /// Logger flag, set to False to log a notice on the SINGLE next increment of m_countCircularBufferOverruns
    bool m_printedCbTooSmallNotice;
    /// Whether m_udpPacketSendDelayTimer is currently active
    bool m_sendDelayTimerIsRunning;
    /// Whether currently simulating LOS
    bool m_isStateLossOfSignal;
    /// Packet drop simulation state update flag, whether a state update operation is currently active
    volatile bool m_setUdpDropSimulatorFunctionInProgress;
    /// Start of LOS
    const uint64_t m_lossOfSignalStartMs;
    /// Duration of LOS
    const boost::posix_time::time_duration m_lossOfSignalDuration;
    /// count of LOS timer starts (for making sure LOS only occurs once)
    unsigned int m_countLossOfSignalTimerStarts;
    /// Packet drop simulation state update condition variable
    boost::condition_variable m_cvSetUdpDropSimulatorFunction;
    /// Packet drop simulation state update mutex
    boost::mutex m_mutexSetUdpDropSimulatorFunction;
    /// Packet drop simulation function
    UdpDropSimulatorFunction_t m_udpDropSimulatorFunction;

    //stats
    /// The value of m_countTotalUdpPacketsReceived frozen for next stat tracking window
    uint64_t m_lastTotalUdpPacketsReceived;
    /// The value of m_countTotalUdpBytesReceived frozen for next stat tracking window
    uint64_t m_lastTotalUdpBytesReceived;
    /// The value of m_countTotalUdpPacketsSent frozen for next stat tracking window
    uint64_t m_lastTotalUdpPacketsSent;
    /// The value of m_countTotalUdpBytesSent frozen for next stat tracking window
    uint64_t m_lastTotalUdpBytesSent;
public:
    /// Total number of requests attempted to queue a packet for transmission while transmission buffers were full
    uint64_t m_countCircularBufferOverruns;
    /// Maximum recorded number of packets queued for transmission at a single point in time
    uint64_t m_countMaxCircularBufferSize;
    /// Total number of UDP packets received from origin
    uint64_t m_countTotalUdpPacketsReceived;
    /// Total number of UDP bytes received from origin
    uint64_t m_countTotalUdpBytesReceived;
    /// Total number of UDP packets forwarded to destination
    uint64_t m_countTotalUdpPacketsSent;
    /// Total number of UDP bytes forwarded to destination
    uint64_t m_countTotalUdpBytesSent;
};

#endif  //_UDP_DELAY_SIM_H
