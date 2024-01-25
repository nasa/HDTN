/**
 * @file LtpIpcEngine.h
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
 * This LtpIpcEngine class is a child class of LtpEngine.
 * It implements LTP over Interprocess Communication (IPC).
 * It is used for benchmarking ONLY.
 * Its purpose is to find the theoretical
 * max rate between two LTP processes
 * by completely bypassing the OS UDP network layer.
 */


#ifndef _LTP_IPC_ENGINE_H
#define _LTP_IPC_ENGINE_H 1
#define BOOST_INTERPROCESS_FORCE_NATIVE_EMULATION
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <map>
#include <queue>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "LtpEngine.h"
#include "LtpEngineConfig.h"
#include <atomic>

class CLASS_VISIBILITY_LTP_LIB LtpIpcEngine : public LtpEngine {
private:
    LtpIpcEngine() = delete;
public:
    struct IpcPacket {
        //unsigned int dataIndex; //don't use a data pointer since logical address may be different across processes
        unsigned int bytesTransferred;
    };

    struct IpcControl
    {
        IpcControl() = delete;
        IpcControl(const unsigned int numCbElements, const uint64_t bytesPerElement)
            : m_bytesPerElement(bytesPerElement),
            m_waitUntilNotFull_postHasFreeSpace_semaphore(numCbElements - 1),
            m_waitUntilNotEmpty_postHasData_semaphore(0),
            m_circularIndexBuffer(numCbElements)
        {}

        const uint64_t m_bytesPerElement;
        //Semaphores to protect and synchronize access
        boost::interprocess::interprocess_semaphore m_waitUntilNotFull_postHasFreeSpace_semaphore;
        boost::interprocess::interprocess_semaphore m_waitUntilNotEmpty_postHasData_semaphore;
        /// Circular index buffer, used to index the circular vector of receive buffers
        CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    private:
        uint64_t forceEnd64BitBoundary;
    };


    LTP_LIB_EXPORT LtpIpcEngine(
        const std::string& myTxSharedMemoryName,
        const uint64_t maxUdpRxPacketSizeBytes,
        const LtpEngineConfig& ltpRxOrTxCfg);


    LTP_LIB_EXPORT virtual ~LtpIpcEngine() override;

    LTP_LIB_EXPORT bool Connect(const std::string& remoteTxSharedMemoryName);

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

    LTP_LIB_NO_EXPORT void DoSendPacket(const std::vector<boost::asio::const_buffer>& constBufferVec);

    LTP_LIB_NO_EXPORT void ReadRemoteTxShmThreadFunc();
    LTP_LIB_NO_EXPORT bool VerifyIpcPacketReceive(uint8_t* data, std::size_t bytesTransferred);
    
    std::unique_ptr<boost::thread> m_readRemoteTxShmThreadPtr;

    
    const std::string m_myTxSharedMemoryName;
    const uint64_t M_REMOTE_ENGINE_ID;
    std::unique_ptr<boost::interprocess::shared_memory_object> m_myTxSharedMemoryObjectPtr;
    std::unique_ptr<boost::interprocess::mapped_region> m_myTxShmMappedRegionPtr;
    IpcControl* m_myTxIpcControlPtr;
    IpcPacket* m_myTxIpcPacketCbArray;
    uint8_t* m_myTxIpcDataStart;

    std::string m_remoteTxSharedMemoryName;
    std::unique_ptr<boost::interprocess::shared_memory_object> m_remoteTxSharedMemoryObjectPtr;
    std::unique_ptr<boost::interprocess::mapped_region> m_remoteTxShmMappedRegionPtr;
    IpcControl* m_remoteTxIpcControlPtr;
    IpcPacket* m_remoteTxIpcPacketCbArray;
    uint8_t* m_remoteTxIpcDataStart;

    /// Number of receive buffers
    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;


    
    

    /// Logger flag, set to False to log a notice on the SINGLE next increment of m_countCircularBufferOverruns
    std::atomic<bool> m_running;
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



#endif //_LTP_IPC_ENGINE_H
