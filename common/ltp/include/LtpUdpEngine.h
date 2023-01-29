/**
 * @file LtpUdpEngine.h
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
 * This LtpUdpEngine class is a child class of LtpEngine.
 * It manages a reference to a bidirectional udp socket
 * and a circular buffer of incoming UDP packets to feed into the LtpEngine.
 */


#ifndef _LTP_UDP_ENGINE_H
#define _LTP_UDP_ENGINE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <map>
#include <queue>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "LtpEngine.h"
#include "UdpBatchSender.h"
#include "LtpEngineConfig.h"

class CLASS_VISIBILITY_LTP_LIB LtpUdpEngine : public LtpEngine {
private:
    LtpUdpEngine() = delete;
public:
    /**
     * Preallocate M_NUM_CIRCULAR_BUFFER_VECTORS receive buffers, each with a size of maxUdpRxPacketSizeBytes.
     * If the engine is configured for more than one packet per system call, initialize the batch sender instance passing LtpUdpEngine::OnSentPacketsCallback as the send callback.
     * @param ioServiceUdpRef I/O execution context reference, typically supplied by the associated LtpUdpEngineManager.
     * @param udpSocketRef Our managed UDP socket, typically supplied by the associated LtpUdpEngineManager.
     * @param engineIndexForEncodingIntoRandomSessionNumber The engine index.
     * @param remoteEndpoint The remote UDP endpoint.
     * @param maxUdpRxPacketSizeBytes The maximum UDP packet size in bytes.
     * @param ltpRxOrTxCfg The engine config.
     */
    LTP_LIB_EXPORT LtpUdpEngine(boost::asio::io_service & ioServiceUdpRef,
        boost::asio::ip::udp::socket & udpSocketRef,
        const uint8_t engineIndexForEncodingIntoRandomSessionNumber, 
        const boost::asio::ip::udp::endpoint & remoteEndpoint,
        const uint64_t maxUdpRxPacketSizeBytes,
        const LtpEngineConfig& ltpRxOrTxCfg);

    /// Stat logging
    LTP_LIB_EXPORT virtual ~LtpUdpEngine() override;
    
    /** Initiate an engine reset (thread-safe).
     *
     * Initiates an asynchronous call to LtpUdpEngine::Reset() to perform a reset.
     * The calling thread blocks until the request is resolved.
     */
    LTP_LIB_EXPORT void Reset_ThreadSafe_Blocking();
    
    /** Perform engine reset.
     *
     * Calls LtpEngine::Reset() to reset the underlying LTP engine, then clear tracked stats.
     */
    LTP_LIB_EXPORT virtual void Reset() override;
    
    /**
     * If the receive buffers are full, the packet is dropped.
     * Else, swaps in the packet to the receive buffer and calls LtpEngine::PacketIn_ThreadSafe() to forward to the underlying LtpEngine for processing.
     * packetIn_thenSwappedForAnotherSameSizeVector holds the entire receive buffer, only the first 'size' bytes belong in the current packet and are
     * therefore safe to read, any bytes that follow may contain partial data from previously processed packets and should be treated as garbage values.
     * @param packetIn_thenSwappedForAnotherSameSizeVector The packet to transmit.
     * @param size The size of the packet.
     * @post The argument to packetIn_thenSwappedForAnotherSameSizeVector is swapped with a potentially dirty buffer of an already-processed packet and is ready to
     * be reused as the receive buffer for the next read operation (see above how to handle dirty buffers).
     */
    LTP_LIB_EXPORT void PostPacketFromManager_ThreadSafe(std::vector<uint8_t> & packetIn_thenSwappedForAnotherSameSizeVector, std::size_t size);

    /** Initiate a request to set the UDP endpoint (thread-safe).
     *
     * Initiates an asynchronous request to LtpUdpEngine::SetEndpoint().
     * @param remoteEndpoint The UDP endpoint to set.
     */
    LTP_LIB_EXPORT void SetEndpoint_ThreadSafe(const boost::asio::ip::udp::endpoint& remoteEndpoint);
    
    /** Initiate a request to set the UDP endpoint (thread-safe).
     *
     * Initiates an asynchronous request to LtpUdpEngine::SetEndpoint().
     * @param remoteHostname The remote host.
     * @param remotePort The remote port.
     */
    LTP_LIB_EXPORT void SetEndpoint_ThreadSafe(const std::string& remoteHostname, const uint16_t remotePort);

    LTP_LIB_EXPORT bool ReadyToSend() const noexcept;

private:
    /** Handle the completion of a receive buffer processing operation.
     *
     * Invoked by the underlying LTP engine when a received packet is fully processed.
     * Completes the processing by committing the read to the circular index buffer.
     * @param success Whether the processing was successful.
     */
    LTP_LIB_NO_EXPORT virtual void PacketInFullyProcessedCallback(bool success) override;
    
    /** Initiate a UDP send operation for a single packet.
     *
     * Initiates an asynchronous send operation with LtpUdpEngine::HandleUdpSend() as a completion handler.
     * @param constBufferVec The data buffers to send.
     * @param underlyingDataToDeleteOnSentCallback The underlying data buffers shared pointer.
     * @param underlyingCsDataToDeleteOnSentCallback The underlying client service data to send shared pointer.
     * @post The arguments to underlyingDataToDeleteOnSentCallback and underlyingCsDataToDeleteOnSentCallback are left in a moved-from state.
     */
    LTP_LIB_NO_EXPORT virtual void SendPacket(const std::vector<boost::asio::const_buffer>& constBufferVec,
        std::shared_ptr<std::vector<std::vector<uint8_t> > >&& underlyingDataToDeleteOnSentCallback,
        std::shared_ptr<LtpClientServiceDataToSend>&& underlyingCsDataToDeleteOnSentCallback) override;
    
    /** Handle a UDP send operation.
     *
     * Invoked for single-packet-per-system-call send operations.
     * If successful, signals the underlying LtpEngine that a system call has completed with OnSendPacketsSystemCallCompleted_ThreadSafe().
     * @param underlyingDataToDeleteOnSentCallback The underlying data buffers shared pointer.
     * @param underlyingCsDataToDeleteOnSentCallback The underlying client service data to send shared pointer.
     * @param error The error code.
     * @param bytes_transferred The number of bytes sent.
     */
    LTP_LIB_NO_EXPORT void HandleUdpSend(std::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback,
        std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback,
        const boost::system::error_code& error, std::size_t bytes_transferred);
    
    /** Initiate a batch UDP send operation.
     *
     * Queues the packets for a batch send operation with UdpBatchSender::QueueSendPacketsOperation_ThreadSafe().
     * The batch sender instance is configured to use LtpUdpEngine::OnSentPacketsCallback() as a completion handler which will be called once the operation has completed.
     * @param constBufferVecs The vector of data buffers to send.
     * @param underlyingDataToDeleteOnSentCallback The vector of underlying data buffers shared pointers.
     * @param underlyingCsDataToDeleteOnSentCallback The vector of underlying client service data to send shared pointers.
     */
    LTP_LIB_NO_EXPORT virtual void SendPackets(std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend) override;
    
    /** Handle a batch UDP send operation.
     *
     * Invoked for batch send operations.
     * If successful, signals the underlying LtpEngine that a system call has completed with OnSendPacketsSystemCallCompleted_ThreadSafe().
     * @param success Whether the operation was successful.
     * @param constBufferVecs The vector of data buffers to send.
     * @param underlyingDataToDeleteOnSentCallback The vector of underlying data buffers shared pointers.
     * @param underlyingCsDataToDeleteOnSentCallback The vector of underlying client service data to send shared pointers.
     */
    LTP_LIB_NO_EXPORT void OnSentPacketsCallback(bool success, std::shared_ptr<std::vector<UdpSendPacketInfo> >& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsSent);
    
    /** Set the UDP endpoint.
     *
     * If the engine is configured for more than one packet per system call, also sets the endpoint for the batch sender instance.
     * @param remoteEndpoint The UDP endpoint to set.
     */
    LTP_LIB_NO_EXPORT void SetEndpoint(const boost::asio::ip::udp::endpoint& remoteEndpoint);
    
    /** Set the UDP endpoint.
     *
     * Tries to synchronously resolve the remote endpoint from the given host/port pair, if successful calls LtpUdpEngine::SetEndpoint(resolved_endpoint).
     * @param remoteHostname The remote host.
     * @param remotePort The remote port.
     * @post If the resolve operation fails, the UDP endpoint is NOT set and the operation is silently ignored.
     */
    LTP_LIB_NO_EXPORT void SetEndpoint(const std::string& remoteHostname, const uint16_t remotePort);
    

    
    /// Batch sender instance
    UdpBatchSender m_udpBatchSenderConnected;
    /// I/O execution context reference, typically supplied by the associated LtpUdpEngineManager
    boost::asio::io_service & m_ioServiceUdpRef;
    /// Our managed UDP socket, typically supplied by the associated LtpUdpEngineManager
    boost::asio::ip::udp::socket & m_udpSocketRef;
    /// Remote UDP endpoint
    boost::asio::ip::udp::endpoint m_remoteEndpoint;

    /// Number of receive buffers
    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    /// Circular index buffer, used to index the circular vector of receive buffers
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    /// Circular vector of receive buffers
    std::vector<std::vector<boost::uint8_t> > m_udpReceiveBuffersCbVec;

    /// Logger flag, set to False to log a notice on the SINGLE next increment of m_countCircularBufferOverruns
    bool m_printedCbTooSmallNotice;
    volatile bool m_printedUdpSendFailedNotice;

    //for safe unit test resets
    /// Whether an engine reset is currently in progress
    volatile bool m_resetInProgress;
    /// Engine reset mutex
    boost::mutex m_resetMutex;
    /// Engine reset condition variable
    boost::condition_variable m_resetConditionVariable;

public:
    /// Total number of initiated send operations
    volatile uint64_t m_countAsyncSendCalls;
    /// Total number of send operation completion handler invocations, indicates the number of completed send operations
    volatile uint64_t m_countAsyncSendCallbackCalls; //same as udp packets sent
    /// Total number of initiated batch send operations through m_udpBatchSenderConnected
    volatile uint64_t m_countBatchSendCalls;
    /// Total number of batch send operation completion handler invocations, indicates the number of completed batch send operations
    volatile uint64_t m_countBatchSendCallbackCalls;
    /// Total number of packets actually sent across batch send operations
    volatile uint64_t m_countBatchUdpPacketsSent;
    //total udp packets sent is m_countAsyncSendCallbackCalls + m_countBatchUdpPacketsSent

    /// Total number of requests attempted to queue a packet for transmission while transmission buffers were full
    uint64_t m_countCircularBufferOverruns;
    /// Total number of packets received, includes number of dropped packets due to receive buffers being full
    uint64_t m_countUdpPacketsReceived;
};



#endif //_LTP_UDP_ENGINE_H
