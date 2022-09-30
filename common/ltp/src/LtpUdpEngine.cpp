/**
 * @file LtpUdpEngine.cpp
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

#include "LtpUdpEngine.h"
#include <boost/make_unique.hpp>


LtpUdpEngine::LtpUdpEngine(boost::asio::io_service & ioServiceUdpRef, boost::asio::ip::udp::socket & udpSocketRef,
    const uint64_t thisEngineId, const uint8_t engineIndexForEncodingIntoRandomSessionNumber,
    const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const boost::asio::ip::udp::endpoint & remoteEndpoint, const unsigned int numUdpRxCircularBufferVectors,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, const uint64_t maxRedRxBytesPerSession, uint32_t checkpointEveryNthDataPacketSender,
    uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers, const uint64_t maxUdpRxPacketSizeBytes, const uint64_t maxSendRateBitsPerSecOrZeroToDisable,
    const uint64_t maxSimultaneousSessions, const uint64_t rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable,
    const uint64_t maxUdpPacketsToSendPerSystemCall, const uint64_t senderPingSecondsOrZeroToDisable) :
    LtpEngine(thisEngineId, engineIndexForEncodingIntoRandomSessionNumber, mtuClientServiceData, mtuReportSegment, oneWayLightTime, oneWayMarginTime,
        ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, maxRedRxBytesPerSession, true, checkpointEveryNthDataPacketSender, maxRetriesPerSerialNumber,
        force32BitRandomNumbers, maxSendRateBitsPerSecOrZeroToDisable, maxSimultaneousSessions,
        rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable, maxUdpPacketsToSendPerSystemCall, senderPingSecondsOrZeroToDisable),
    m_ioServiceUdpRef(ioServiceUdpRef),
    m_udpSocketRef(udpSocketRef),
    m_remoteEndpoint(remoteEndpoint),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numUdpRxCircularBufferVectors),
    M_MAX_UDP_RX_PACKET_SIZE_BYTES(maxUdpRxPacketSizeBytes),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_printedCbTooSmallNotice(false),
    m_countAsyncSendCalls(0),
    m_countAsyncSendCallbackCalls(0),
    m_countBatchSendCalls(0),
    m_countBatchSendCallbackCalls(0),
    m_countBatchUdpPacketsSent(0),
    m_countCircularBufferOverruns(0)
{
    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(maxUdpRxPacketSizeBytes);
    }

    if (M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL > 1) { //need a dedicated connected sender socket
        m_udpBatchSenderConnected.SetOnSentPacketsCallback(boost::bind(&LtpUdpEngine::OnSentPacketsCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));
        if (!m_udpBatchSenderConnected.Init(m_remoteEndpoint)) {
            std::cout << "Error in LtpUdpEngine::LtpUdpEngine: could not init dedicated udp batch sender socket\n";
        }
        else {
            std::cout << "LtpUdpEngine successfully initialized dedicated udp batch sender socket to send up to "
                << M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL << " udp packets per system call\n";
        }
    }
}

LtpUdpEngine::~LtpUdpEngine() {
    //std::cout << "end of ~LtpUdpEngine with port " << M_MY_BOUND_UDP_PORT << std::endl;
    std::cout << "~LtpUdpEngine: m_countAsyncSendCalls " << m_countAsyncSendCalls 
        << " m_countBatchSendCalls " << m_countBatchSendCalls
        << " m_countBatchUdpPacketsSent " << m_countBatchUdpPacketsSent
        << " m_countCircularBufferOverruns " << m_countCircularBufferOverruns << std::endl;
}


void LtpUdpEngine::Reset() {
    LtpEngine::Reset();
    m_countAsyncSendCalls = 0;
    m_countAsyncSendCallbackCalls = 0;
    m_countBatchSendCalls = 0;
    m_countBatchSendCallbackCalls = 0;
    m_countBatchUdpPacketsSent = 0;
    m_countCircularBufferOverruns = 0;
}


void LtpUdpEngine::PostPacketFromManager_ThreadSafe(std::vector<uint8_t> & packetIn_thenSwappedForAnotherSameSizeVector, std::size_t size) {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
        ++m_countCircularBufferOverruns;
        if (!m_printedCbTooSmallNotice) {
            m_printedCbTooSmallNotice = true;
            std::cout << "notice in LtpUdpEngine::StartUdpReceive(): buffers full.. you might want to increase the circular buffer size! Next UDP packet will be dropped!" << std::endl;
        }
    }
    else {
        packetIn_thenSwappedForAnotherSameSizeVector.swap(m_udpReceiveBuffersCbVec[writeIndex]);
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        PacketIn_ThreadSafe(m_udpReceiveBuffersCbVec[writeIndex].data(), size); //Post to the LtpEngine IoService so its thread will process
    }
}

void LtpUdpEngine::SendPacket(
    std::vector<boost::asio::const_buffer> & constBufferVec,
    std::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback)
{
    //called by LtpEngine Thread
    ++m_countAsyncSendCalls;
    if (m_udpDropSimulatorFunction && m_udpDropSimulatorFunction(*((uint8_t*)constBufferVec[0].data()))) {
        boost::asio::post(m_ioServiceUdpRef, boost::bind(&LtpUdpEngine::HandleUdpSend, this, underlyingDataToDeleteOnSentCallback, underlyingCsDataToDeleteOnSentCallback, boost::system::error_code(), 0));
    }
    else {
        m_udpSocketRef.async_send_to(constBufferVec, m_remoteEndpoint,
            boost::bind(&LtpUdpEngine::HandleUdpSend, this, std::move(underlyingDataToDeleteOnSentCallback), std::move(underlyingCsDataToDeleteOnSentCallback),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}

void LtpUdpEngine::SendPackets(std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
    std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
    std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec)
{
    //called by LtpEngine Thread
    ++m_countBatchSendCalls;
    
    if (m_udpDropSimulatorFunction) {
        for (std::size_t i = 0; i < constBufferVecs.size(); ++i) {
            if (m_udpDropSimulatorFunction(*((uint8_t*)constBufferVecs[i][0].data()))) { //dropped
                constBufferVecs[i].clear(); //this packet will be skipped by the UdpBatchSender
            }
        }
    }
    m_udpBatchSenderConnected.QueueSendPacketsOperation_ThreadSafe(constBufferVecs, underlyingDataToDeleteOnSentCallbackVec, underlyingCsDataToDeleteOnSentCallbackVec); //data gets stolen
    //LtpUdpEngine::OnSentPacketsCallback will be called next
}


void LtpUdpEngine::PacketInFullyProcessedCallback(bool success) {
    //Called by LTP Engine thread
    //std::cout << "PacketInFullyProcessedCallback " << std::endl;
    m_circularIndexBuffer.CommitRead(); //LtpEngine IoService thread will CommitRead
}



void LtpUdpEngine::HandleUdpSend(std::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback,
    const boost::system::error_code& error, std::size_t bytes_transferred)
{
    //Called by m_ioServiceUdpRef thread
    ++m_countAsyncSendCallbackCalls;
    if (error) {
        std::cerr << "error in LtpUdpEngine::HandleUdpSend: " << error.message() << std::endl;
        //DoUdpShutdown();
    }
    else {
        //rate stuff handled in LtpEngine due to self-sending nature of LtpEngine
        //std::cout << "sent " << bytes_transferred << std::endl;

        if (m_countAsyncSendCallbackCalls == m_countAsyncSendCalls) { //prevent too many sends from stacking up in ioService queue
            SignalReadyForSend_ThreadSafe();
        }
    }
}

void LtpUdpEngine::OnSentPacketsCallback(bool success, std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
    std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallback,
    std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallback)
{
    //Called by UdpBatchSender thread
    ++m_countBatchSendCallbackCalls;
    m_countBatchUdpPacketsSent += constBufferVecs.size();
    if (!success) {
        std::cerr << "error in LtpUdpEngine::OnSentPacketsCallback\n";
        //DoUdpShutdown();
    }
    else {
        //rate stuff handled in LtpEngine due to self-sending nature of LtpEngine
        //std::cout << "sent " << bytes_transferred << std::endl;

        if (m_countBatchSendCallbackCalls == m_countBatchSendCalls) { //prevent too many sends from stacking up in UdpBatchSender queue
            SignalReadyForSend_ThreadSafe();
        }
    }
}
