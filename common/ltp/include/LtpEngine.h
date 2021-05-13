#ifndef LTP_ENGINE_H
#define LTP_ENGINE_H 1

#include <boost/random/random_device.hpp>
#include <boost/thread.hpp>
#include "LtpFragmentMap.h"
#include "Ltp.h"
#include "LtpRandomNumberGenerator.h"
#include "LtpSessionReceiver.h"
#include "LtpSessionSender.h"

class LtpEngine {
private:
    LtpEngine();
public:
    struct transmission_request_t {
        uint64_t destinationClientServiceId;
        uint64_t destinationLtpEngineId;
        std::vector<uint8_t> clientServiceDataToSend;
        uint64_t lengthOfRedPart;
    };
    
    LtpEngine(const uint64_t thisEngineId, const uint64_t mtuClientServiceData, 
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, bool startIoServiceThread = false);

    ~LtpEngine();

    void Reset();
    void SetCheckpointEveryNthDataPacketForSenders(uint64_t checkpointEveryNthDataPacketSender);

    void TransmissionRequest(boost::shared_ptr<transmission_request_t> & transmissionRequest);
    void TransmissionRequest_ThreadSafe(boost::shared_ptr<transmission_request_t> && transmissionRequest);
    void TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId, std::vector<uint8_t> && clientServiceDataToSend, uint64_t lengthOfRedPart);
    void TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId, const uint8_t * clientServiceDataToCopyAndSend, uint64_t length, uint64_t lengthOfRedPart);
    
    void SetRedPartReceptionCallback(const RedPartReceptionCallback_t & callback);

    bool PacketIn(const uint8_t * data, const std::size_t size);
    bool PacketIn(const std::vector<boost::asio::const_buffer> & constBufferVec); //for testing

    void PacketIn_ThreadSafe(const uint8_t * data, const std::size_t size);
    void PacketIn_ThreadSafe(const std::vector<boost::asio::const_buffer> & constBufferVec); //for testing

    bool NextPacketToSendRoundRobin(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback);

protected:
    virtual void PacketInFullyProcessedCallback(bool success);
    virtual void SendPacket(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback);
    void SignalReadyForSend_ThreadSafe();
private:
    void TrySendPacketIfAvailable();

    void CancelSegmentReceivedCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode, bool isFromSender,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    void CancelAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, bool isToSender,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    void ReportAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, uint64_t reportSerialNumberBeingAcknowledged,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    void ReportSegmentReceivedCallback(const Ltp::session_id_t & sessionId, const Ltp::report_segment_t & reportSegment,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    void DataSegmentReceivedCallback(uint8_t segmentTypeFlags, const Ltp::session_id_t & sessionId,
        std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
private:
    Ltp m_ltpRxStateMachine;
    LtpRandomNumberGenerator m_rng;
    const uint64_t M_THIS_ENGINE_ID;
    const uint64_t M_MTU_CLIENT_SERVICE_DATA;
    const boost::posix_time::time_duration M_ONE_WAY_LIGHT_TIME;
    const boost::posix_time::time_duration M_ONE_WAY_MARGIN_TIME;
    const boost::posix_time::time_duration M_TRANSMISSION_TO_ACK_RECEIVED_TIME;
    boost::random_device m_randomDevice;
    //boost::mutex m_randomDeviceMutex;
    std::map<uint64_t, std::unique_ptr<LtpSessionSender> > m_mapSessionNumberToSessionSender;
    std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> > m_mapSessionIdToSessionReceiver;

    std::map<uint64_t, std::unique_ptr<LtpSessionSender> >::iterator m_sendersIterator;
    std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator m_receiversIterator;

    RedPartReceptionCallback_t m_redPartReceptionCallback;
    uint64_t m_checkpointEveryNthDataPacketSender;

    boost::asio::io_service m_ioServiceLtpEngine; //for timers and post calls only
    boost::asio::io_service::work m_workLtpEngine;
    std::unique_ptr<boost::thread> m_ioServiceLtpEngineThreadPtr;
};

#endif // LTP_ENGINE_H

