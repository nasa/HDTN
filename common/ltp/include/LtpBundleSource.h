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
#include <unordered_set>
#include <vector>
#include "Telemetry.h"
#include "LtpUdpEngineManager.h"
#include "LtpEngineConfig.h"
#include "BundleCallbackFunctionDefines.h"
#include <zmq.hpp>
#include <atomic>
#include <boost/core/noncopyable.hpp>

class LtpBundleSource : private boost::noncopyable {
private:
    LtpBundleSource() = delete;
public:
    LTP_LIB_EXPORT LtpBundleSource(const LtpEngineConfig& ltpTxCfg);

    LTP_LIB_EXPORT ~LtpBundleSource();
    LTP_LIB_EXPORT void Stop();
    LTP_LIB_EXPORT bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData);
    LTP_LIB_EXPORT bool Forward(zmq::message_t & dataZmq, std::vector<uint8_t>&& userData);
    LTP_LIB_EXPORT bool Forward(std::vector<uint8_t> & dataVec, std::vector<uint8_t>&& userData);
    LTP_LIB_EXPORT std::size_t GetTotalDataSegmentsAcked();
    LTP_LIB_EXPORT std::size_t GetTotalDataSegmentsSent();
    LTP_LIB_EXPORT std::size_t GetTotalDataSegmentsUnacked();
    //std::size_t GetTotalBundleBytesAcked();
    LTP_LIB_EXPORT std::size_t GetTotalBundleBytesSent();
    //std::size_t GetTotalBundleBytesUnacked();
    LTP_LIB_EXPORT void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback);
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
    const uint64_t M_BUNDLE_PIPELINE_LIMIT;
    std::unordered_set<uint64_t> m_activeSessionNumbersSet;
    std::atomic<unsigned int> m_startingCount;

    boost::mutex m_removeEngineMutex;
    boost::condition_variable m_removeEngineCv;
    volatile bool m_removeEngineInProgress;
public:
    //ltp stats
    LtpOutductTelemetry_t m_ltpOutductTelemetry;
};



#endif //_LTP_BUNDLE_SOURCE_H
