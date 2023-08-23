/**
 * @file LtpEncapLocalStreamEngine.h
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
 * This LtpEncapLocalStreamEngine class is a child class of LtpEngine.
 * It implements LTP over an Encap local stream.
 */


#ifndef _LTP_ENCAP_LOCAL_STREAM_ENGINE_H
#define _LTP_ENCAP_LOCAL_STREAM_ENGINE_H 1
#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <map>
#include <queue>
#include <array>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "LtpEngine.h"
#include "UdpBatchSender.h"
#include "LtpEngineConfig.h"
#include <atomic>
#include "AsyncDuplexLocalStream.h"

class CLASS_VISIBILITY_LTP_LIB LtpEncapLocalStreamEngine : public LtpEngine {
private:
    LtpEncapLocalStreamEngine() = delete;
public:
    struct SendElement {
        std::size_t numPacketsToSend;
        std::vector<std::array<uint8_t, 8> > m_encapHeaders;
        std::vector<boost::asio::const_buffer> constBufferVec;
        std::shared_ptr<std::vector<std::vector<uint8_t> > > underlyingDataToDeleteOnSentCallback;
        std::shared_ptr<LtpClientServiceDataToSend> underlyingCsDataToDeleteOnSentCallback;
        //batch ops
        std::shared_ptr<std::vector<UdpSendPacketInfo> > udpSendPacketInfoVecSharedPtr;

        SendElement() {
            Reset();
        }
        void Reset() {
            numPacketsToSend = 1;
            m_encapHeaders.resize(1); //always minimum size
            constBufferVec.resize(0);
            underlyingDataToDeleteOnSentCallback.reset();
            underlyingCsDataToDeleteOnSentCallback.reset();
            udpSendPacketInfoVecSharedPtr.reset();
        }
    };
    


    LTP_LIB_EXPORT LtpEncapLocalStreamEngine(
        const uint64_t maxEncapRxPacketSizeBytes,
        const LtpEngineConfig& ltpRxOrTxCfg);


    LTP_LIB_EXPORT virtual ~LtpEncapLocalStreamEngine() override;

    LTP_LIB_EXPORT bool Connect(const std::string& socketOrPipePath, bool isStreamCreator);

    LTP_LIB_EXPORT void Stop();
    
    /** Initiate an engine reset (thread-safe).
     *
     * Initiates an asynchronous call to LtpIpcEngine::Reset() to perform a reset.
     * The calling thread blocks until the request is resolved.
     */
    LTP_LIB_EXPORT void Reset_ThreadSafe_Blocking();
    
    /** Perform engine reset.
     *
     * Calls LtpEngine::Reset() to reset the underlying LTP engine, then clear tracked stats.
     */
    LTP_LIB_EXPORT virtual void Reset() override;
    
    
    
    LTP_LIB_EXPORT bool ReadyToSend() const noexcept;

private:
    /** Handle the completion of a receive buffer processing operation.
     *
     * Invoked by the underlying LTP engine when a received packet is fully processed.
     * Completes the processing by committing the read to the circular index buffer.
     * @param success Whether the processing was successful.
     */
    LTP_LIB_NO_EXPORT virtual void PacketInFullyProcessedCallback(bool success) override;
    
    /** Initiate a UDP send operation for a single packet. //TODO: Docs, complete
     *
     * Initiates an asynchronous send operation with LtpUdpEngine::HandleUdpSend() as a completion handler.
     * @param constBufferVec
     * @param underlyingDataToDeleteOnSentCallback
     * @param underlyingCsDataToDeleteOnSentCallback
     * @post The arguments to underlyingDataToDeleteOnSentCallback and underlyingCsDataToDeleteOnSentCallback are left in a moved-from state.
     */
    LTP_LIB_NO_EXPORT virtual void SendPacket(const std::vector<boost::asio::const_buffer> & constBufferVec,
        std::shared_ptr<std::vector<std::vector<uint8_t> > >&& underlyingDataToDeleteOnSentCallback,
        std::shared_ptr<LtpClientServiceDataToSend>&& underlyingCsDataToDeleteOnSentCallback) override;
    
    
    
    /** Initiate a batch UDP send operation. //TODO: Docs, complete
     *
     * Stub to make compatible with LtpEngine, calls DoSendPacket as many times as needed.
     * @param constBufferVecs
     * @param underlyingDataToDeleteOnSentCallback
     * @param underlyingCsDataToDeleteOnSentCallback
     */
    LTP_LIB_NO_EXPORT virtual void SendPackets(std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend) override;

   

    LTP_LIB_NO_EXPORT void TrySendOperationIfAvailable_NotThreadSafe();
    LTP_LIB_NO_EXPORT void HandleSendOperationCompleted(
        const boost::system::error_code& error, std::size_t bytes_transferred, const unsigned int consumeIndex);


    LTP_LIB_NO_EXPORT bool VerifyLtpPacketReceive(const uint8_t* data, std::size_t bytesTransferred);

    LTP_LIB_NO_EXPORT void OnFullEncapPacketReceived(padded_vector_uint8_t& receivedFullEncapPacket,
        uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize);
    
    AsyncDuplexLocalStream m_asyncDuplexLocalStream;

    
    const uint64_t M_REMOTE_ENGINE_ID;


    const unsigned int MAX_TX_SEND_SYSTEM_CALLS_IN_FLIGHT;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_txCb;
    std::vector<SendElement> m_txCbVec;
    bool m_writeInProgress;
    bool m_sendErrorOccurred;

    
    

    /// Logger flag, set to False to log a notice on the SINGLE next increment of m_countCircularBufferOverruns
    const bool m_isInduct;

    //for safe unit test resets
    /// Whether an engine reset is currently in progress
    std::atomic<bool> m_resetInProgress;
    /// Engine reset mutex
    boost::mutex m_resetMutex;
    /// Engine reset condition variable
    boost::condition_variable m_resetConditionVariable;

public:
    /// Total number of initiated send operations
    std::atomic<uint64_t> m_countAsyncSendCalls;
    /// Total number of send operation completion handler invocations, indicates the number of completed send operations
    std::atomic<uint64_t> m_countAsyncSendCallbackCalls; //same as udp packets sent
    /// Total number of initiated batch send operations through m_udpBatchSenderConnected
    std::atomic<uint64_t> m_countBatchSendCalls;
    /// Total number of batch send operation completion handler invocations, indicates the number of completed batch send operations
    std::atomic<uint64_t> m_countBatchSendCallbackCalls;
    /// Total number of packets actually sent across batch send operations
    std::atomic<uint64_t> m_countBatchUdpPacketsSent;
    //total udp packets sent is m_countAsyncSendCallbackCalls + m_countBatchUdpPacketsSent

    /// Total number of requests attempted to queue a packet for transmission while transmission buffers were full
    std::atomic<uint64_t> m_countCircularBufferOverruns;
    /// Total number of packets received, includes number of dropped packets due to receive buffers being full
    std::atomic<uint64_t> m_countUdpPacketsReceived;
};



#endif //_LTP_ENCAP_LOCAL_STREAM_ENGINE_H
