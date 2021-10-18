#ifndef _LTP_BUNDLE_SINK_H
#define _LTP_BUNDLE_SINK_H 1

#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include "LtpUdpEngineManager.h"

class LtpBundleSink {
private:
    LtpBundleSink();
public:
    typedef boost::function<void(std::vector<uint8_t> & wholeBundleVec)> LtpWholeBundleReadyCallback_t;

    LtpBundleSink(const LtpWholeBundleReadyCallback_t & ltpWholeBundleReadyCallback,
        const uint64_t thisEngineId, const uint64_t expectedSessionOriginatorEngineId, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const uint16_t myBoundUdpPort, const unsigned int numUdpRxCircularBufferVectors,
        const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION,
        uint32_t ltpMaxRetriesPerSerialNumber, const bool force32BitRandomNumbers,
        const std::string & remoteUdpHostname, const uint16_t remoteUdpPort, const uint64_t maxBundleSizeBytes);
    ~LtpBundleSink();
    bool ReadyToBeDeleted();
private:
    void RemoveCallback();

    //tcpcl received data callback functions
    void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec,
        uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock);
    void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode);

    const LtpWholeBundleReadyCallback_t m_ltpWholeBundleReadyCallback;

    //ltp vars
    const uint64_t M_THIS_ENGINE_ID;
    const uint64_t M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;
    LtpUdpEngineManager * const m_ltpUdpEngineManagerPtr;
    LtpUdpEngine * m_ltpUdpEnginePtr;

    volatile bool m_removeCallbackCalled;
};



#endif  //_LTP_BUNDLE_SINK_H
