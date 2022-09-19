/**
 * @file LtpBundleSource.h
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
 *
 * @section DESCRIPTION
 *
 * This LtpBundleSource class encapsulates the appropriate LTP functionality
 * to send a pipeline of bundles (or any other user defined data) over an LTP over UDP link
 * and calls the user defined function OnSuccessfulAckCallback_t when the session closes, meaning
 * a bundle is fully sent (i.e. the ltp fully red session gets acknowledged by the remote receiver).
 */

#ifndef _LTP_BUNDLE_SOURCE_H
#define _LTP_BUNDLE_SOURCE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <set>
#include <vector>
#include "Telemetry.h"
#include "LtpUdpEngineManager.h"
#include "BundleCallbackFunctionDefines.h"
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
    LTP_LIB_EXPORT LtpBundleSource(const uint64_t clientServiceId, const uint64_t remoteLtpEngineId, const uint64_t thisEngineId, const uint64_t mtuClientServiceData,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const uint16_t myBoundUdpPort, const unsigned int numUdpRxCircularBufferVectors,
        uint32_t checkpointEveryNthDataPacketSender, uint32_t ltpMaxRetriesPerSerialNumber, const bool force32BitRandomNumbers,
        const std::string & remoteUdpHostname, const uint16_t remoteUdpPort, const uint64_t maxSendRateBitsPerSecOrZeroToDisable,
        const uint32_t bundlePipelineLimit, const uint64_t maxUdpPacketsToSendPerSystemCall, const uint64_t senderPingSecondsOrZeroToDisable);

    LTP_LIB_EXPORT ~LtpBundleSource();
    LTP_LIB_EXPORT void Stop();
    LTP_LIB_EXPORT bool Forward(const uint8_t* bundleData, const std::size_t size);
    LTP_LIB_EXPORT bool Forward(zmq::message_t & dataZmq);
    LTP_LIB_EXPORT bool Forward(std::vector<uint8_t> & dataVec);
    LTP_LIB_EXPORT std::size_t GetTotalDataSegmentsAcked();
    LTP_LIB_EXPORT std::size_t GetTotalDataSegmentsSent();
    LTP_LIB_EXPORT std::size_t GetTotalDataSegmentsUnacked();
    //std::size_t GetTotalBundleBytesAcked();
    LTP_LIB_EXPORT std::size_t GetTotalBundleBytesSent();
    //std::size_t GetTotalBundleBytesUnacked();
    LTP_LIB_EXPORT void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
    LTP_LIB_EXPORT void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback);
    LTP_LIB_EXPORT void SetUserAssignedUuid(uint64_t userAssignedUuid);
    LTP_LIB_EXPORT void SyncTelemetry();
private:
    LTP_LIB_NO_EXPORT void RemoveCallback();

    //ltp callback functions for a sender
    LTP_LIB_NO_EXPORT void SessionStartCallback(const Ltp::session_id_t & sessionId);
    LTP_LIB_NO_EXPORT void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId);
    LTP_LIB_NO_EXPORT void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId);
    LTP_LIB_NO_EXPORT void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode);

    volatile bool m_useLocalConditionVariableAckReceived;
    boost::condition_variable m_localConditionVariableAckReceived;

    //ltp vars
    std::shared_ptr<LtpUdpEngineManager> m_ltpUdpEngineManagerPtr;
    LtpUdpEngine * m_ltpUdpEnginePtr;
    const uint64_t M_CLIENT_SERVICE_ID;
    const uint64_t M_THIS_ENGINE_ID;
    const uint64_t M_REMOTE_LTP_ENGINE_ID;
    const uint32_t M_BUNDLE_PIPELINE_LIMIT;
    std::set<Ltp::session_id_t> m_activeSessionsSet;

    OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;

    volatile bool m_removeCallbackCalled;
public:
    //ltp stats
    LtpOutductTelemetry_t m_ltpOutductTelemetry;
};



#endif //_LTP_BUNDLE_SOURCE_H
