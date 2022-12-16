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
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpUdpEngine::LtpUdpEngine(boost::asio::io_service& ioServiceUdpRef,
    boost::asio::ip::udp::socket& udpSocketRef,
    const uint8_t engineIndexForEncodingIntoRandomSessionNumber,
    const boost::asio::ip::udp::endpoint& remoteEndpoint,
    const uint64_t maxUdpRxPacketSizeBytes,
    const LtpEngineConfig& ltpRxOrTxCfg) :
    LtpEngine(ltpRxOrTxCfg, engineIndexForEncodingIntoRandomSessionNumber, true),
    m_ioServiceUdpRef(ioServiceUdpRef),
    m_udpSocketRef(udpSocketRef),
    m_remoteEndpoint(remoteEndpoint),
    M_NUM_CIRCULAR_BUFFER_VECTORS(ltpRxOrTxCfg.numUdpRxCircularBufferVectors),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_printedCbTooSmallNotice(false),
    m_countAsyncSendCalls(0),
    m_countAsyncSendCallbackCalls(0),
    m_countBatchSendCalls(0),
    m_countBatchSendCallbackCalls(0),
    m_countBatchUdpPacketsSent(0),
    m_countCircularBufferOverruns(0),
    m_countUdpPacketsReceived(0)
{
    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(maxUdpRxPacketSizeBytes);
    }

    if (M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL > 1) { //need a dedicated connected sender socket
        m_udpBatchSenderConnected.SetOnSentPacketsCallback(boost::bind(&LtpUdpEngine::OnSentPacketsCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));
        if (!m_udpBatchSenderConnected.Init(m_remoteEndpoint)) {
            LOG_ERROR(subprocess) << "LtpUdpEngine::LtpUdpEngine: could not init dedicated udp batch sender socket";
        }
        else {
            LOG_INFO(subprocess) << "LtpUdpEngine successfully initialized dedicated udp batch sender socket to send up to "
                << M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL << " udp packets per system call";
        }
    }
}

LtpUdpEngine::~LtpUdpEngine() {
    LOG_INFO(subprocess) << "~LtpUdpEngine: m_countAsyncSendCalls " << m_countAsyncSendCalls 
        << " m_countBatchSendCalls " << m_countBatchSendCalls
        << " m_countBatchUdpPacketsSent " << m_countBatchUdpPacketsSent
        << " m_countCircularBufferOverruns " << m_countCircularBufferOverruns
        << " m_countUdpPacketsReceived " << m_countUdpPacketsReceived;
}

void LtpUdpEngine::Reset_ThreadSafe_Blocking() {
    boost::mutex::scoped_lock cvLock(m_resetMutex);
    m_resetInProgress = true;
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpUdpEngine::Reset, this));
    while (m_resetInProgress) { //lock mutex (above) before checking condition
        m_resetConditionVariable.wait(cvLock);
    }
}

void LtpUdpEngine::Reset() {
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


void LtpUdpEngine::PostPacketFromManager_ThreadSafe(std::vector<uint8_t> & packetIn_thenSwappedForAnotherSameSizeVector, std::size_t size) {
    ++m_countUdpPacketsReceived;
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
        ++m_countCircularBufferOverruns;
        if (!m_printedCbTooSmallNotice) {
            m_printedCbTooSmallNotice = true;
            LOG_WARNING(subprocess) << "LtpUdpEngine::StartUdpReceive(): buffers full.. you might want to increase the circular buffer size! Next UDP packet will be dropped!";
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
    m_udpSocketRef.async_send_to(constBufferVec, m_remoteEndpoint,
        boost::bind(&LtpUdpEngine::HandleUdpSend, this, std::move(underlyingDataToDeleteOnSentCallback), std::move(underlyingCsDataToDeleteOnSentCallback),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
}

void LtpUdpEngine::SendPackets(std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
    std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
    std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec)
{
    //called by LtpEngine Thread
    ++m_countBatchSendCalls;
    m_udpBatchSenderConnected.QueueSendPacketsOperation_ThreadSafe(constBufferVecs, underlyingDataToDeleteOnSentCallbackVec, underlyingCsDataToDeleteOnSentCallbackVec); //data gets stolen
    //LtpUdpEngine::OnSentPacketsCallback will be called next
}


void LtpUdpEngine::PacketInFullyProcessedCallback(bool success) {
    //Called by LTP Engine thread
    m_circularIndexBuffer.CommitRead(); //LtpEngine IoService thread will CommitRead
}



void LtpUdpEngine::HandleUdpSend(std::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback,
    const boost::system::error_code& error, std::size_t bytes_transferred)
{
    //Called by m_ioServiceUdpRef thread
    ++m_countAsyncSendCallbackCalls;
    if (error) {
        LOG_ERROR(subprocess) << "LtpUdpEngine::HandleUdpSend: " << error.message();
        //DoUdpShutdown();
    }
    else {
        //rate stuff handled in LtpEngine due to self-sending nature of LtpEngine

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
        LOG_ERROR(subprocess) << "LtpUdpEngine::OnSentPacketsCallback";
        //DoUdpShutdown();
    }
    else {
        //rate stuff handled in LtpEngine due to self-sending nature of LtpEngine

        if (m_countBatchSendCallbackCalls == m_countBatchSendCalls) { //prevent too many sends from stacking up in UdpBatchSender queue
            SignalReadyForSend_ThreadSafe();
        }
    }
}

void LtpUdpEngine::SetEndpoint_ThreadSafe(const boost::asio::ip::udp::endpoint& remoteEndpoint) {
    //m_ioServiceLtpEngine is the only running thread that uses m_remoteEndpoint
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpUdpEngine::SetEndpoint, this, remoteEndpoint));
}
void LtpUdpEngine::SetEndpoint_ThreadSafe(const std::string& remoteHostname, const uint16_t remotePort) {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpUdpEngine::SetEndpoint, this, remoteHostname, remotePort));
}
void LtpUdpEngine::SetEndpoint(const boost::asio::ip::udp::endpoint& remoteEndpoint) {
    m_remoteEndpoint = remoteEndpoint;
    if (M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL > 1) { //using dedicated connected sender socket
        m_udpBatchSenderConnected.SetEndpointAndReconnect_ThreadSafe(m_remoteEndpoint);
    }
}
void LtpUdpEngine::SetEndpoint(const std::string& remoteHostname, const uint16_t remotePort) {
    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    LOG_INFO(subprocess) << "LtpUdpEngine resolving " << remoteHostname << ":" << remotePort;

    boost::asio::ip::udp::endpoint udpDestinationEndpoint;
    {
        boost::asio::ip::udp::resolver resolver(m_ioServiceLtpEngine);
        try {
            udpDestinationEndpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), remoteHostname, boost::lexical_cast<std::string>(remotePort), UDP_RESOLVER_FLAGS));
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "LtpUdpEngine::SetEndpoint: " << e.what() << "  code=" << e.code();
            return;
        }
    }
    SetEndpoint(udpDestinationEndpoint);
}
