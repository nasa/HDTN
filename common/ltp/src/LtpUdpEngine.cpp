#include "LtpUdpEngine.h"
#include <boost/make_unique.hpp>


LtpUdpEngine::LtpUdpEngine(boost::asio::io_service & ioServiceUdpRef, boost::asio::ip::udp::socket & udpSocketRef,
    const uint64_t thisEngineId, const uint8_t engineIndexForEncodingIntoRandomSessionNumber,
    const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const boost::asio::ip::udp::endpoint & remoteEndpoint, const unsigned int numUdpRxCircularBufferVectors,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, const uint64_t maxRedRxBytesPerSession, uint32_t checkpointEveryNthDataPacketSender,
    uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers, const uint64_t maxUdpRxPacketSizeBytes, const uint64_t maxSendRateBitsPerSecOrZeroToDisable) :
    LtpEngine(thisEngineId, engineIndexForEncodingIntoRandomSessionNumber, mtuClientServiceData, mtuReportSegment, oneWayLightTime, oneWayMarginTime,
        ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, maxRedRxBytesPerSession, true, checkpointEveryNthDataPacketSender, maxRetriesPerSerialNumber, force32BitRandomNumbers, maxSendRateBitsPerSecOrZeroToDisable),
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
    m_countCircularBufferOverruns(0)
{
    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(maxUdpRxPacketSizeBytes);
    }
}

LtpUdpEngine::~LtpUdpEngine() {
    //std::cout << "end of ~LtpUdpEngine with port " << M_MY_BOUND_UDP_PORT << std::endl;
    std::cout << "~LtpUdpEngine: m_countAsyncSendCalls " << m_countAsyncSendCalls << " m_countCircularBufferOverruns " << m_countCircularBufferOverruns << std::endl;
}


void LtpUdpEngine::Reset() {
    LtpEngine::Reset();
    m_countAsyncSendCalls = 0;
    m_countAsyncSendCallbackCalls = 0;
    m_countCircularBufferOverruns = 0;
}


void LtpUdpEngine::PostPacketFromManager_ThreadSafe(std::vector<uint8_t> & packetIn_thenSwappedForAnotherSameSizeVector, std::size_t size) {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == UINT32_MAX) {
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

void LtpUdpEngine::SendPacket(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback, const uint64_t sessionOriginatorEngineId) {
    //called by LtpEngine Thread
    ++m_countAsyncSendCalls;
    if (m_udpDropSimulatorFunction && m_udpDropSimulatorFunction(*((uint8_t*)constBufferVec[0].data()))) {
        boost::asio::post(m_ioServiceUdpRef, boost::bind(&LtpUdpEngine::HandleUdpSend, this, underlyingDataToDeleteOnSentCallback, boost::system::error_code(), 0));
    }
    else {
        m_udpSocketRef.async_send_to(constBufferVec, m_remoteEndpoint,
            boost::bind(&LtpUdpEngine::HandleUdpSend, this, std::move(underlyingDataToDeleteOnSentCallback),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}


void LtpUdpEngine::PacketInFullyProcessedCallback(bool success) {
    //Called by LTP Engine thread
    //std::cout << "PacketInFullyProcessedCallback " << std::endl;
    m_circularIndexBuffer.CommitRead(); //LtpEngine IoService thread will CommitRead
}



void LtpUdpEngine::HandleUdpSend(boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback, const boost::system::error_code& error, std::size_t bytes_transferred) {
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
