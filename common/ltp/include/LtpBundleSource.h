#ifndef _LTP_BUNDLE_SOURCE_H
#define _LTP_BUNDLE_SOURCE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <set>
#include <vector>
#include "LtpUdpEngine.h"
#include <zmq.hpp>

class LtpBundleSource {
private:
    LtpBundleSource();
public:
    typedef boost::function<void()> OnSuccessfulAckCallback_t;
    LtpBundleSource(const uint64_t clientServiceId, const uint64_t remoteLtpEngineId, const uint64_t thisEngineId, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const uint16_t udpPort = 0, const bool senderRequireRemoteEndpointMatchOnReceivePacket = true, const unsigned int numUdpRxCircularBufferVectors = 100,
        const unsigned int maxUdpRxPacketSizeBytes = UINT16_MAX, const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION = 0,
        uint32_t checkpointEveryNthDataPacketSender = 0, uint32_t ltpMaxRetriesPerSerialNumber = 5, const bool force32BitRandomNumbers = false);

    ~LtpBundleSource();
    void Stop();
    bool Forward(const uint8_t* bundleData, const std::size_t size);
    bool Forward(zmq::message_t & dataZmq);
    bool Forward(std::vector<uint8_t> & dataVec);
    std::size_t GetTotalDataSegmentsAcked();
    std::size_t GetTotalDataSegmentsSent();
    std::size_t GetTotalDataSegmentsUnacked();
    //std::size_t GetTotalBundleBytesAcked();
    std::size_t GetTotalBundleBytesSent();
    //std::size_t GetTotalBundleBytesUnacked();
    void Connect(const std::string & hostname, const std::string & port);
    bool ReadyToForward();
    void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
private:


    //ltp callback functions for a sender
    void SessionStartCallback(const Ltp::session_id_t & sessionId);
    void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId);
    void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId);
    void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode);

    volatile bool m_useLocalConditionVariableAckReceived;
    boost::condition_variable m_localConditionVariableAckReceived;

    //ltp vars
    LtpUdpEngine m_ltpUdpEngine;
    const uint64_t M_CLIENT_SERVICE_ID;
    const uint64_t M_REMOTE_LTP_ENGINE_ID;
    std::set<Ltp::session_id_t> m_activeSessionsSet;

    OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;


public:
    //ltp stats
    std::size_t m_totalDataSegmentsSentSuccessfullyWithAck;
    std::size_t m_totalDataSegmentsFailedToSend;
    std::size_t m_totalDataSegmentsSent;
    std::size_t m_totalBundleBytesSent;
};



#endif //_LTP_BUNDLE_SOURCE_H
