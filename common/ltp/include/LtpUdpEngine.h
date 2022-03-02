#ifndef _LTP_UDP_ENGINE_H
#define _LTP_UDP_ENGINE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <map>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "LtpEngine.h"

class CLASS_VISIBILITY_LTP_LIB LtpUdpEngine : public LtpEngine {
private:
    LtpUdpEngine();
public:
    typedef boost::function<bool(const uint8_t ltpHeaderByte)> UdpDropSimulatorFunction_t;
    
    LTP_LIB_EXPORT LtpUdpEngine(boost::asio::io_service & ioServiceUdpRef, boost::asio::ip::udp::socket & udpSocketRef, const uint64_t thisEngineId,
        const uint8_t engineIndexForEncodingIntoRandomSessionNumber, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, 
        const boost::asio::ip::udp::endpoint & remoteEndpoint, const unsigned int numUdpRxCircularBufferVectors,
        const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, const uint64_t maxRedRxBytesPerSession, uint32_t checkpointEveryNthDataPacketSender,
        uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers, const uint64_t maxUdpRxPacketSizeBytes);

    LTP_LIB_EXPORT virtual ~LtpUdpEngine();

    LTP_LIB_EXPORT virtual void Reset();
    
    LTP_LIB_EXPORT void PostPacketFromManager_ThreadSafe(std::vector<uint8_t> & packetIn_thenSwappedForAnotherSameSizeVector, std::size_t size);
private:
    LTP_LIB_NO_EXPORT virtual void PacketInFullyProcessedCallback(bool success);
    LTP_LIB_NO_EXPORT virtual void SendPacket(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback, const uint64_t sessionOriginatorEngineId);
    LTP_LIB_NO_EXPORT void HandleUdpSend(boost::shared_ptr<std::vector<std::vector<uint8_t> > > underlyingDataToDeleteOnSentCallback, const boost::system::error_code& error, std::size_t bytes_transferred);


    
    
    boost::asio::io_service & m_ioServiceUdpRef;
    boost::asio::ip::udp::socket & m_udpSocketRef;
    boost::asio::ip::udp::endpoint m_remoteEndpoint;
    

    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    const uint64_t M_MAX_UDP_RX_PACKET_SIZE_BYTES;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::vector<boost::uint8_t> > m_udpReceiveBuffersCbVec;

    bool m_printedCbTooSmallNotice;
public:
    volatile uint64_t m_countAsyncSendCalls;
    volatile uint64_t m_countAsyncSendCallbackCalls;
    uint64_t m_countCircularBufferOverruns;

    //unit testing drop packet simulation stuff
    UdpDropSimulatorFunction_t m_udpDropSimulatorFunction;
   // boost::asio::ip::udp::endpoint m_udpDestinationNullEndpoint;
};



#endif //_LTP_UDP_ENGINE_H
