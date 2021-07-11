#ifndef _LTP_BUNDLE_SOURCE_H
#define _LTP_BUNDLE_SOURCE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <set>
#include <vector>
#include "LtpUdpEngineManager.h"
#include <zmq.hpp>

class LtpBundleSource {
private:
    LtpBundleSource();
public:
    typedef boost::function<void()> OnSuccessfulAckCallback_t;
    /*
    const LtpWholeBundleReadyCallback_t & ltpWholeBundleReadyCallback,
        const uint64_t thisEngineId, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const uint16_t myBoundUdpPort, const unsigned int numUdpRxCircularBufferVectors,
        const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION,
        uint32_t ltpMaxRetriesPerSerialNumber, const bool force32BitRandomNumbers,
        const std::string & remoteUdpHostname, const uint16_t remoteUdpPort*/
    LtpBundleSource(const uint64_t clientServiceId, const uint64_t remoteLtpEngineId, const uint64_t thisEngineId, const uint64_t mtuClientServiceData,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const uint16_t myBoundUdpPort, const unsigned int numUdpRxCircularBufferVectors,
        uint32_t checkpointEveryNthDataPacketSender, uint32_t ltpMaxRetriesPerSerialNumber, const bool force32BitRandomNumbers,
        const std::string & remoteUdpHostname, const uint16_t remoteUdpPort);

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
    void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
private:
    void RemoveCallback();

    //ltp callback functions for a sender
    void SessionStartCallback(const Ltp::session_id_t & sessionId);
    void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId);
    void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId);
    void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode);

    volatile bool m_useLocalConditionVariableAckReceived;
    boost::condition_variable m_localConditionVariableAckReceived;

    //ltp vars
    LtpUdpEngineManager * const m_ltpUdpEngineManagerPtr;
    LtpUdpEngine * m_ltpUdpEnginePtr;
    const uint64_t M_CLIENT_SERVICE_ID;
    const uint64_t M_THIS_ENGINE_ID;
    const uint64_t M_REMOTE_LTP_ENGINE_ID;
    std::set<Ltp::session_id_t> m_activeSessionsSet;

    OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;

    volatile bool m_removeCallbackCalled;
public:
    //ltp stats
    std::size_t m_totalDataSegmentsSentSuccessfullyWithAck;
    std::size_t m_totalDataSegmentsFailedToSend;
    std::size_t m_totalDataSegmentsSent;
    std::size_t m_totalBundleBytesSent;
};



#endif //_LTP_BUNDLE_SOURCE_H
