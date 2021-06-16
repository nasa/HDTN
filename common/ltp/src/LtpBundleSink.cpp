#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "LtpBundleSink.h"
#include <boost/make_unique.hpp>

LtpBundleSink::LtpBundleSink(const LtpWholeBundleReadyCallback_t & ltpWholeBundleReadyCallback,
    const uint64_t thisEngineId, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const uint16_t udpPort, const unsigned int numUdpRxCircularBufferVectors, const unsigned int maxUdpRxPacketSizeBytes, const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION) :

    m_ltpWholeBundleReadyCallback(ltpWholeBundleReadyCallback),
    m_ltpUdpEngine(thisEngineId, mtuClientServiceData, mtuReportSegment, oneWayLightTime, oneWayMarginTime,
        udpPort, numUdpRxCircularBufferVectors, maxUdpRxPacketSizeBytes, ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION)
{
    m_ltpUdpEngine.SetRedPartReceptionCallback(boost::bind(&LtpBundleSink::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5));
    m_ltpUdpEngine.SetReceptionSessionCancelledCallback(boost::bind(&LtpBundleSink::ReceptionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));
}

LtpBundleSink::~LtpBundleSink() {}



void LtpBundleSink::RedPartReceptionCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec,
    uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock)
{
    m_ltpWholeBundleReadyCallback(movableClientServiceDataVec);

    //This function is holding up the LtpEngine thread.  Once this red part reception callback exits, the last LTP checkpoint report segment (ack)
    //can be sent to the sending ltp engine to close the session
}


void LtpBundleSink::ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode)
{
    std::cout << "remote has cancelled session " << sessionId << " with reason code " << (int)reasonCode << std::endl;
}



bool LtpBundleSink::ReadyToBeDeleted() {
    return true;
}
