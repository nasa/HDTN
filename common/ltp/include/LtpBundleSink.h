#ifndef _LTP_BUNDLE_SINK_H
#define _LTP_BUNDLE_SINK_H 1

#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include "LtpUdpEngine.h"

class LtpBundleSink {
private:
    LtpBundleSink();
public:
    typedef boost::function<void(std::vector<uint8_t> & wholeBundleVec)> LtpWholeBundleReadyCallback_t;

    LtpBundleSink(const LtpWholeBundleReadyCallback_t & ltpWholeBundleReadyCallback,
        const uint64_t thisEngineId, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const uint16_t udpPort = 0, const unsigned int numUdpRxCircularBufferVectors = 100,
        const unsigned int maxUdpRxPacketSizeBytes = UINT16_MAX, const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION = 0);
    ~LtpBundleSink();
    bool ReadyToBeDeleted();
private:


    //tcpcl received data callback functions
    void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec,
        uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock);
    void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode);

    const LtpWholeBundleReadyCallback_t m_ltpWholeBundleReadyCallback;

    //ltp vars
    LtpUdpEngine m_ltpUdpEngine;
};



#endif  //_LTP_BUNDLE_SINK_H
