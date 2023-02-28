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
 * to send a pipeline of bundles (or any other user defined data) over an LTP link (transport layer must be defined in child class)
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
#include "TelemetryDefinitions.h"
#include "LtpEngine.h"
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

    LTP_LIB_EXPORT virtual ~LtpBundleSource();
    LTP_LIB_EXPORT bool Init();
    LTP_LIB_EXPORT void Stop();
    LTP_LIB_EXPORT bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData);
    LTP_LIB_EXPORT bool Forward(zmq::message_t & dataZmq, std::vector<uint8_t>&& userData);
    LTP_LIB_EXPORT bool Forward(std::vector<uint8_t> & dataVec, std::vector<uint8_t>&& userData);
    LTP_LIB_EXPORT std::size_t GetTotalDataSegmentsAcked();
    LTP_LIB_EXPORT std::size_t GetTotalDataSegmentsSent();
    LTP_LIB_EXPORT std::size_t GetTotalDataSegmentsUnacked();
    LTP_LIB_EXPORT std::size_t GetTotalBundleBytesAcked();
    LTP_LIB_EXPORT std::size_t GetTotalBundleBytesSent();
    //std::size_t GetTotalBundleBytesUnacked();
    LTP_LIB_EXPORT void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback);
    LTP_LIB_EXPORT void SetUserAssignedUuid(uint64_t userAssignedUuid);
    LTP_LIB_EXPORT void SetRate(uint64_t maxSendRateBitsPerSecOrZeroToDisable);
    LTP_LIB_EXPORT void SyncTelemetry();
    LTP_LIB_EXPORT uint64_t GetOutductMaxNumberOfBundlesInPipeline() const;
    
protected:
    LTP_LIB_EXPORT virtual bool ReadyToForward() = 0;
    LTP_LIB_EXPORT virtual bool SetLtpEnginePtr() = 0;
    LTP_LIB_EXPORT virtual void SyncTransportLayerSpecificTelem() = 0;
private:
    

    //ltp callback functions for a sender
    LTP_LIB_NO_EXPORT void SessionStartCallback(const Ltp::session_id_t & sessionId);
    LTP_LIB_NO_EXPORT void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId);
    LTP_LIB_NO_EXPORT void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId);
    LTP_LIB_NO_EXPORT void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode);

    volatile bool m_useLocalConditionVariableAckReceived;
    boost::condition_variable m_localConditionVariableAckReceived;

    //ltp vars
protected:
    const LtpEngineConfig m_ltpTxCfg;
    LtpEngine * m_ltpEnginePtr;
    const uint64_t M_CLIENT_SERVICE_ID;
    const uint64_t M_THIS_ENGINE_ID;
    const uint64_t M_REMOTE_LTP_ENGINE_ID;
    const uint64_t M_BUNDLE_PIPELINE_LIMIT;
private:
    std::unordered_set<uint64_t> m_activeSessionNumbersSet;
    std::atomic<unsigned int> m_startingCount;

    
public:
    //ltp stats
    LtpOutductTelemetry_t m_ltpOutductTelemetry;
};



#endif //_LTP_BUNDLE_SOURCE_H
