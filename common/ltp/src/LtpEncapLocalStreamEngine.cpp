/**
 * @file LtpEncapLocalStreamEngine.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "LtpEncapLocalStreamEngine.h"
#include "CcsdsEncapEncode.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include "Sdnv.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static const uint8_t ENGINE_INDEX = 1; //this is a don't care for inducts, only needed for outducts (to mimic udp behavior)

LtpEncapLocalStreamEngine::LtpEncapLocalStreamEngine(const uint64_t maxEncapRxPacketSizeBytes,
    const LtpEngineConfig& ltpRxOrTxCfg) :
    LtpEngine(ltpRxOrTxCfg, ENGINE_INDEX, true),
    m_encapAsyncDuplexLocalStream(m_ioServiceLtpEngine, //ltp engine will handle i/o, keeping entirely single threaded
        ENCAP_PACKET_TYPE::LTP,
        maxEncapRxPacketSizeBytes,
        boost::bind(&LtpEncapLocalStreamEngine::OnFullEncapPacketReceived, this,
            boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&LtpEncapLocalStreamEngine::OnLocalStreamConnectionStatusChanged, this,
            boost::placeholders::_1),
        true), //true => keep encap header
    M_REMOTE_ENGINE_ID(ltpRxOrTxCfg.remoteEngineId),
    MAX_TX_SEND_SYSTEM_CALLS_IN_FLIGHT(6), //from LtpEngine.cpp MAX_QUEUED_SEND_SYSTEM_CALLS = 5;
    m_txCb(MAX_TX_SEND_SYSTEM_CALLS_IN_FLIGHT),
    m_txCbVec(MAX_TX_SEND_SYSTEM_CALLS_IN_FLIGHT),
    m_writeInProgress(false),
    m_sendErrorOccurred(false),
    //M_NUM_CIRCULAR_BUFFER_VECTORS(ltpRxOrTxCfg.numUdpRxCircularBufferVectors),
    
    m_isInduct(ltpRxOrTxCfg.isInduct),
    m_resetInProgress(false),
    m_countAsyncSendCalls(0),
    m_countAsyncSendCallbackCalls(0),
    m_countBatchSendCalls(0),
    m_countBatchSendCallbackCalls(0),
    m_countBatchUdpPacketsSent(0),
    m_countCircularBufferOverruns(0),
    m_countUdpPacketsReceived(0)
{
    //LOG_INFO(subprocess) << "LtpIpcEngine using " << M_NUM_CIRCULAR_BUFFER_VECTORS << " circular buffer elements";
    
    //logic from LtpUdpEngineManager::AddLtpUdpEngine
    if ((ltpRxOrTxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable != 0) && (!m_isInduct)) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: delaySendingOfReportSegmentsTimeMsOrZeroToDisable must be set to 0 for an outduct";
        return;
    }
    if ((ltpRxOrTxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable != 0) && (m_isInduct)) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: delaySendingOfDataSegmentsTimeMsOrZeroToDisable must be set to 0 for an induct";
        return;
    }
    if (ltpRxOrTxCfg.maxUdpPacketsToSendPerSystemCall == 0) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: maxUdpPacketsToSendPerSystemCall must be non-zero.";
        return;
    }
#ifdef UIO_MAXIOV
    //sendmmsg() is Linux-specific. NOTES The value specified in vlen is capped to UIO_MAXIOV (1024).
    if (ltpRxOrTxCfg.maxUdpPacketsToSendPerSystemCall > UIO_MAXIOV) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: maxUdpPacketsToSendPerSystemCall ("
            << ltpRxOrTxCfg.maxUdpPacketsToSendPerSystemCall << ") must be <= UIO_MAXIOV (" << UIO_MAXIOV << ").";
        return;
    }
#endif //UIO_MAXIOV
    if (ltpRxOrTxCfg.senderPingSecondsOrZeroToDisable && m_isInduct) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: senderPingSecondsOrZeroToDisable cannot be used with an induct (must be set to 0).";
        return;
    }
    if (ltpRxOrTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable != 0) { //if enabled
        if (ltpRxOrTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable < 1000) {
            LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable, if non-zero, must be 1000ms minimum.";
            return;
        }
        else if (ltpRxOrTxCfg.activeSessionDataOnDiskDirectory.empty()) {
            LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: activeSessionDataOnDiskDirectory must be defined when activeSessionDataOnDisk feature is enabled.";
            return;
        }
        else if (ltpRxOrTxCfg.maxSimultaneousSessions < 8) { //because of "static constexpr uint64_t diskPipelineLimit = 6;" in LtpEngine.cpp, want a number greater than 6
            LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: when activeSessionDataOnDisk feature is enabled, maxSimultaneousSessions must be at least 8.";
            return;
        }
    }
}

LtpEncapLocalStreamEngine::~LtpEncapLocalStreamEngine() {
    Stop();

    LOG_INFO(subprocess) << "~LtpEncapLocalStreamEngine:"
        << "\n m_countAsyncSendCalls " << m_countAsyncSendCalls 
        << "\n m_countBatchSendCalls " << m_countBatchSendCalls
        << "\n m_countBatchUdpPacketsSent " << m_countBatchUdpPacketsSent
        << "\n m_countCircularBufferOverruns " << m_countCircularBufferOverruns
        << "\n m_countUdpPacketsReceived " << m_countUdpPacketsReceived;
}

void LtpEncapLocalStreamEngine::Stop() {
    m_encapAsyncDuplexLocalStream.Stop();
}

bool LtpEncapLocalStreamEngine::Connect(const std::string& socketOrPipePath, bool isStreamCreator) {
    return m_encapAsyncDuplexLocalStream.Init(socketOrPipePath, isStreamCreator);
}

void LtpEncapLocalStreamEngine::Reset_ThreadSafe_Blocking() {
    boost::mutex::scoped_lock cvLock(m_resetMutex);
    m_resetInProgress = true;
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEncapLocalStreamEngine::Reset, this));
    while (m_resetInProgress) { //lock mutex (above) before checking condition
        m_resetConditionVariable.wait(cvLock);
    }
}

void LtpEncapLocalStreamEngine::Reset() {
    LtpEngine::Reset();
    m_countAsyncSendCalls = 0;
    m_countAsyncSendCallbackCalls = 0;
    m_countBatchSendCalls = 0;
    m_countBatchSendCallbackCalls = 0;
    m_countBatchUdpPacketsSent = 0;
    m_countCircularBufferOverruns = 0;
    m_countUdpPacketsReceived = 0;
    m_resetMutex.lock();
    m_resetInProgress = false;
    m_resetMutex.unlock();
    m_resetConditionVariable.notify_one();
}

//called by the LtpEngine thread (can use LtpEngine non-thread-safe function calls)
void LtpEncapLocalStreamEngine::OnFullEncapPacketReceived(padded_vector_uint8_t& receivedFullEncapPacket,
    uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize)
{
    const uint8_t* const ltpPacketPtr = receivedFullEncapPacket.data() + decodedEncapHeaderSize;
    if (VerifyLtpPacketReceive(ltpPacketPtr, decodedEncapPayloadSize)) {
        if (!PacketIn(true, ltpPacketPtr, decodedEncapPayloadSize)) { //Not thread safe, immediately puts into ltp rx state machine and calls PacketInFullyProcessedCallback
            LOG_ERROR(subprocess) << "LtpEncapLocalStreamEngine: bad packet received";
        }
    }
    m_encapAsyncDuplexLocalStream.StartReadFirstEncapHeaderByte_NotThreadSafe();
}
void LtpEncapLocalStreamEngine::OnLocalStreamConnectionStatusChanged(bool isOnConnectionEvent) {
    LOG_INFO(subprocess) << "LtpEncapLocalStreamEngine connection " << ((isOnConnectionEvent) ? "up" : "down");
    const bool error = !isOnConnectionEvent;
    if (error) {
        if (!m_sendErrorOccurred) {
            m_sendErrorOccurred = true;
            LOG_ERROR(subprocess) << "LtpEncapLocalStreamEngine.. future packets will be dropped";
        }
    }
    else {
        if (m_sendErrorOccurred) {
            m_sendErrorOccurred = false; //trigger these prints on "rising edge"
            LOG_INFO(subprocess) << "LtpEncapLocalStreamEngine sender back to normal operation";
        }
    }
}

void LtpEncapLocalStreamEngine::TrySendOperationIfAvailable_NotThreadSafe() {
    //only do this if idle (no write in progress)
    if (!m_writeInProgress) {
        if (m_sendErrorOccurred) {
            //prevent packet from being sent
        }
        else {
            const unsigned int consumeIndex = m_txCb.GetIndexForRead(); //store the volatile
            if (consumeIndex != CIRCULAR_INDEX_BUFFER_EMPTY) {
                SendElement& el = m_txCbVec[consumeIndex];
                m_writeInProgress = true;
                boost::asio::async_write(m_encapAsyncDuplexLocalStream.GetStreamHandleRef(),
                    el.constBufferVec,
                    boost::bind(&LtpEncapLocalStreamEngine::HandleSendOperationCompleted, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred,
                        consumeIndex));
            }
        }
    }
}
void LtpEncapLocalStreamEngine::HandleSendOperationCompleted(const boost::system::error_code& error,
    std::size_t bytes_transferred, const unsigned int consumeIndex)
{
    (void)bytes_transferred;
    m_writeInProgress = false;
    SendElement& el = m_txCbVec[consumeIndex];
    if (error) {
        if (!m_sendErrorOccurred) {
            m_sendErrorOccurred = true;
            LOG_ERROR(subprocess) << "LtpEncapLocalStreamEngine send failed.. these packet(s) (and subsequent packets) will be dropped: "
                << error.message();
        }
    }
    else {
        if (m_sendErrorOccurred) {
            m_sendErrorOccurred = false; //trigger these prints on "rising edge"
            LOG_INFO(subprocess) << "LtpEncapLocalStreamEngine sender back to normal operation";
        }

        if (el.udpSendPacketInfoVecSharedPtr) { //batch send
            m_countBatchSendCallbackCalls.fetch_add(1, std::memory_order_relaxed);
            m_countBatchUdpPacketsSent.fetch_add(el.numPacketsToSend, std::memory_order_relaxed);
        }
        else { //single send
            m_countAsyncSendCallbackCalls.fetch_add(1, std::memory_order_relaxed);
        }
        el.Reset();
        //notify the ltp engine regardless whether or not there is an error
        //NEW BEHAVIOR (always notify LtpEngine which keeps its own internal count of pending Udp Send system calls queued)
        OnSendPacketsSystemCallCompleted_ThreadSafe(); //per system call operation, not per udp packet(s)

        m_txCb.CommitRead(); //cb is sized 1 larger in case a Forward() is called between notify and CommitRead

        TrySendOperationIfAvailable_NotThreadSafe();
    }
}



void LtpEncapLocalStreamEngine::PacketInFullyProcessedCallback(bool success) {
    (void)success;
    //Called by LTP Engine thread
    //no-op, OnFullEncapPacketReceived calls PacketIn which calls this function
}

bool LtpEncapLocalStreamEngine::VerifyLtpPacketReceive(const uint8_t* data, std::size_t bytesTransferred) {
    if (bytesTransferred <= 2) {
        LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): bytesTransferred <= 2 .. ignoring packet";
        return false;
    }

    const uint8_t segmentTypeFlags = data[0]; // & 0x0f; //upper 4 bits must be 0 for version 0
    bool isSenderToReceiver;
    if (!Ltp::GetMessageDirectionFromSegmentFlags(segmentTypeFlags, isSenderToReceiver)) {
        LOG_FATAL(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): received invalid ltp packet with segment type flag " << (int)segmentTypeFlags;
        return false;
    }
#if defined(USE_SDNV_FAST) && defined(SDNV_SUPPORT_AVX2_FUNCTIONS)
    uint64_t decodedValues[2];
    const unsigned int numSdnvsToDecode = 2u - isSenderToReceiver;
    uint8_t totalBytesDecoded;
    unsigned int numValsDecodedThisIteration = SdnvDecodeMultiple256BitU64Fast(&data[1], &totalBytesDecoded, decodedValues, numSdnvsToDecode);
    if (numValsDecodedThisIteration != numSdnvsToDecode) { //all required sdnvs were not decoded, possibly due to a decode error
        LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): cannot read 1 or more of sessionOriginatorEngineId or sessionNumber.. ignoring packet";
        return false;
    }
    const uint64_t& sessionOriginatorEngineId = decodedValues[0];
    const uint64_t& sessionNumber = decodedValues[1];
#else
    uint8_t sdnvSize;
    const uint64_t sessionOriginatorEngineId = SdnvDecodeU64(&data[1], &sdnvSize, (100 - 1)); //no worries about hardware accelerated sdnv read out of bounds due to minimum 100 byte size
    if (sdnvSize == 0) {
        LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): cannot read sessionOriginatorEngineId.. ignoring packet";
        return false;
    }
    uint64_t sessionNumber;
    if (!isSenderToReceiver) {
        sessionNumber = SdnvDecodeU64(&data[1 + sdnvSize], &sdnvSize, ((100 - 10) - 1)); //no worries about hardware accelerated sdnv read out of bounds due to minimum 100 byte size
        if (sdnvSize == 0) {
            LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): cannot read sessionNumber.. ignoring packet";
            return false;
        }
    }
#endif

    if (isSenderToReceiver) { //received an isSenderToReceiver message type => isInduct (this ltp engine received a message type that only travels from an outduct (sender) to an induct (receiver))
        //sessionOriginatorEngineId is the remote engine id in the case of an induct
        if(sessionOriginatorEngineId != M_REMOTE_ENGINE_ID) {
            LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive: an induct received packet with unknown remote engine Id "
                << sessionOriginatorEngineId << ".. ignoring packet";
            return false;
        }
            
    }
    else { //received an isReceiverToSender message type => isOutduct (this ltp engine received a message type that only travels from an induct (receiver) to an outduct (sender))
        //sessionOriginatorEngineId is my engine id in the case of an outduct.. need to get the session number to find the proper LtpUdpEngine
        const uint8_t engineIndex = LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(sessionNumber);
        if (engineIndex != ENGINE_INDEX) {
            LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive: an outduct received packet of type " << (int)segmentTypeFlags << " with unknown session number "
                << sessionNumber << ".. ignoring packet";
            return false;
        }
    }
    return true;
}




void LtpEncapLocalStreamEngine::SendPacket(
    const std::vector<boost::asio::const_buffer> & constBufferVec,
    std::shared_ptr<std::vector<std::vector<uint8_t> > >&& underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>&& underlyingCsDataToDeleteOnSentCallback)
{
    //called by LtpEngine Thread
    m_countAsyncSendCalls.fetch_add(1, std::memory_order_relaxed);
    
    const unsigned int writeIndex = m_txCb.GetIndexForWrite();
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_FATAL(subprocess) << "LtpEncapLocalStreamEngine::SendPacket.. misconfiguration, tx write buffer full";
        return;
    }

    //el is already reset, this is single threaded
    SendElement& el = m_txCbVec[writeIndex];
    //encode
    std::size_t ltpPayloadSize = 0;
    el.constBufferVec.resize(1 + constBufferVec.size()); //first element for encap header 
    for (std::size_t i = 0; i < constBufferVec.size(); ++i) {
        const boost::asio::const_buffer& b = constBufferVec[i];
        ltpPayloadSize += b.size();
        el.constBufferVec[i + 1] = b;
    }
    //numPacketsToSend = 1; //already set by Reset()
    uint8_t* const encapOutHeader8Byte = &(el.m_encapHeaders[0][0]); //resized to 1 by Reset()
    uint8_t encodedEncapHeaderSize;
    //encode
    if (GetCcsdsEncapHeader(ENCAP_PACKET_TYPE::LTP, encapOutHeader8Byte, static_cast<uint32_t>(ltpPayloadSize), encodedEncapHeaderSize)) {
        el.constBufferVec[0] = boost::asio::buffer(encapOutHeader8Byte, encodedEncapHeaderSize);
        el.underlyingDataToDeleteOnSentCallback = std::move(underlyingDataToDeleteOnSentCallback);
        el.underlyingCsDataToDeleteOnSentCallback = std::move(underlyingCsDataToDeleteOnSentCallback);
        m_txCb.CommitWrite();
        TrySendOperationIfAvailable_NotThreadSafe();
    }
    else {
        LOG_FATAL(subprocess) << "LtpEncapLocalStreamEngine::SendPacket.. unable to encode encap header";
    }
}

void LtpEncapLocalStreamEngine::SendPackets(std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend)
{
    //called by LtpEngine Thread
    m_countBatchSendCalls.fetch_add(1, std::memory_order_relaxed);

    const unsigned int writeIndex = m_txCb.GetIndexForWrite();
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_FATAL(subprocess) << "LtpEncapLocalStreamEngine::SendPackets.. misconfiguration, tx write buffer full";
        return;
    }

    //el is already reset, this is single threaded
    SendElement& el = m_txCbVec[writeIndex];

    std::vector<UdpSendPacketInfo>& udpSendPacketInfoVec = *udpSendPacketInfoVecSharedPtr;
    //for loop should not compare to udpSendPacketInfoVec.size() because that vector may be resized for preallocation,
    // and don't want to everudpSendPacketInfoVec.resize() down because that would call destructor on preallocated UdpSendPacketInfo
    el.numPacketsToSend = numPacketsToSend;
    el.constBufferVec.resize(numPacketsToSend << 2); //reserve max of 4 elements/pieces for each ltp packet, first element for encap header
    el.m_encapHeaders.resize(numPacketsToSend);
    boost::asio::const_buffer* elConstBufPtr = &el.constBufferVec[0];
    for (std::size_t packetI = 0; packetI < numPacketsToSend; ++packetI) {
        const std::vector<boost::asio::const_buffer>& thisConstBufferVec = udpSendPacketInfoVec[packetI].constBufferVec;
        //encode
        std::size_t ltpPayloadSize = 0;
        boost::asio::const_buffer* thisElEncapConstBufPtr = elConstBufPtr++; //first element for encap header
        for (std::size_t i = 0; i < thisConstBufferVec.size(); ++i) {
            const boost::asio::const_buffer& b = thisConstBufferVec[i];
            ltpPayloadSize += b.size();
            *elConstBufPtr++ = b;
        }
        //numPacketsToSend = 1; //already set by Reset()
        uint8_t* const encapOutHeader8Byte = &(el.m_encapHeaders[packetI][0]); //resized to 1 by Reset()
        uint8_t encodedEncapHeaderSize;
        //encode
        if (GetCcsdsEncapHeader(ENCAP_PACKET_TYPE::LTP, encapOutHeader8Byte, static_cast<uint32_t>(ltpPayloadSize), encodedEncapHeaderSize)) {
            *thisElEncapConstBufPtr = boost::asio::buffer(encapOutHeader8Byte, encodedEncapHeaderSize);
        }
        else {
            LOG_FATAL(subprocess) << "LtpEncapLocalStreamEngine::SendPackets.. unable to encode encap header";
            return;
        }
    }
    el.udpSendPacketInfoVecSharedPtr = std::move(udpSendPacketInfoVecSharedPtr);
    m_txCb.CommitWrite();
    TrySendOperationIfAvailable_NotThreadSafe();
}






bool LtpEncapLocalStreamEngine::ReadyToSend() const noexcept {
    return m_encapAsyncDuplexLocalStream.ReadyToSend();
}
