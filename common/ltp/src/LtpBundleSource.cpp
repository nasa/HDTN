/**
 * @file LtpBundleSource.cpp
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
 */

#include <string>
#include "LtpBundleSource.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>
#include <memory>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpBundleSource::LtpBundleSource(const LtpEngineConfig& ltpTxCfg) :

m_useLocalConditionVariableAckReceived(false), //for destructor only

m_ltpUdpEngineManagerPtr(LtpUdpEngineManager::GetOrCreateInstance(ltpTxCfg.myBoundUdpPort, true)),
M_CLIENT_SERVICE_ID(ltpTxCfg.clientServiceId),
M_THIS_ENGINE_ID(ltpTxCfg.thisEngineId),
M_REMOTE_LTP_ENGINE_ID(ltpTxCfg.remoteEngineId),
M_BUNDLE_PIPELINE_LIMIT(ltpTxCfg.maxSimultaneousSessions),
m_startingCount(0),

m_ltpOutductTelemetry()
{
    m_activeSessionNumbersSet.reserve(M_BUNDLE_PIPELINE_LIMIT);
    m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(ltpTxCfg.remoteEngineId, false);
    if (m_ltpUdpEnginePtr == NULL) {
        m_ltpUdpEngineManagerPtr->AddLtpUdpEngine(ltpTxCfg);
        m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(ltpTxCfg.remoteEngineId, false);
    }

    m_ltpUdpEnginePtr->SetSessionStartCallback(boost::bind(&LtpBundleSource::SessionStartCallback, this, boost::placeholders::_1));
    m_ltpUdpEnginePtr->SetTransmissionSessionCompletedCallback(boost::bind(&LtpBundleSource::TransmissionSessionCompletedCallback, this, boost::placeholders::_1));
    m_ltpUdpEnginePtr->SetInitialTransmissionCompletedCallback(boost::bind(&LtpBundleSource::InitialTransmissionCompletedCallback, this, boost::placeholders::_1));
    m_ltpUdpEnginePtr->SetTransmissionSessionCancelledCallback(boost::bind(&LtpBundleSource::TransmissionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));
}

LtpBundleSource::~LtpBundleSource() {
    Stop();
}

void LtpBundleSource::RemoveCallback() {
    m_removeEngineMutex.lock();
    m_removeEngineInProgress = false;
    m_removeEngineMutex.unlock();
    m_removeEngineCv.notify_one();
}

void LtpBundleSource::Stop() {
    if (m_ltpUdpEnginePtr) {
        //prevent TcpclBundleSource from exiting before all bundles sent and acked
        boost::mutex localMutex;
        boost::mutex::scoped_lock lock(localMutex);
        m_useLocalConditionVariableAckReceived = true;
        std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
        for (unsigned int attempt = 0; attempt < 10; ++attempt) {
            const std::size_t numUnacked = GetTotalDataSegmentsUnacked();
            if (numUnacked) {
                LOG_WARNING(subprocess) << "LtpBundleSource destructor waiting on " << numUnacked << " unacked bundles";

                if (previousUnacked > numUnacked) {
                    previousUnacked = numUnacked;
                    attempt = 0;
                }
                m_localConditionVariableAckReceived.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
                //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                continue;
            }
            break;
        }

        LOG_INFO(subprocess) << "waiting to remove ltp bundle source for engine ID " << M_THIS_ENGINE_ID;
        boost::mutex::scoped_lock cvLock(m_removeEngineMutex);
        m_removeEngineInProgress = true;
        m_ltpUdpEngineManagerPtr->RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(M_REMOTE_LTP_ENGINE_ID, false, boost::bind(&LtpBundleSource::RemoveCallback, this));
        while (m_removeEngineInProgress) { //lock mutex (above) before checking condition
            //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
            if (!m_removeEngineCv.timed_wait(cvLock, boost::posix_time::seconds(3))) {
                LOG_ERROR(subprocess) << "timed out waiting (for 3 seconds) to remove ltp bundle source for engine ID " << M_THIS_ENGINE_ID;
                break;
            }
        }
        m_ltpUdpEnginePtr = NULL;

        //print stats
        LOG_INFO(subprocess) << "m_ltpOutductTelemetry.totalBundlesSent " << m_ltpOutductTelemetry.totalBundlesSent;
        LOG_INFO(subprocess) << "m_ltpOutductTelemetry.totalBundlesAcked " << m_ltpOutductTelemetry.totalBundlesAcked;
        LOG_INFO(subprocess) << "m_ltpOutductTelemetry.totalBundleBytesSent " << m_ltpOutductTelemetry.totalBundleBytesSent;
        LOG_INFO(subprocess) << "m_ltpOutductTelemetry.totalBundleBytesAcked " << m_ltpOutductTelemetry.totalBundleBytesAcked;
        LOG_INFO(subprocess) << "m_ltpOutductTelemetry.totalBundlesFailedToSend " << m_ltpOutductTelemetry.totalBundlesFailedToSend;
    }
}

std::size_t LtpBundleSource::GetTotalDataSegmentsAcked() {
    return m_ltpOutductTelemetry.totalBundlesAcked + m_ltpOutductTelemetry.totalBundlesFailedToSend;
}

std::size_t LtpBundleSource::GetTotalDataSegmentsSent() {
    return m_ltpOutductTelemetry.totalBundlesSent;
}

std::size_t LtpBundleSource::GetTotalDataSegmentsUnacked() {
    return GetTotalDataSegmentsSent() - GetTotalDataSegmentsAcked();
}

//std::size_t LtpBundleSource::GetTotalBundleBytesAcked() {
//    return m_totalBytesAcked;
//}

std::size_t LtpBundleSource::GetTotalBundleBytesSent() {
    return m_ltpOutductTelemetry.totalBundleBytesSent;
}

//std::size_t LtpBundleSource::GetTotalBundleBytesUnacked() {
//    return GetTotalBundleBytesSent() - GetTotalBundleBytesAcked();
//}

bool LtpBundleSource::Forward(std::vector<uint8_t> & dataVec, std::vector<uint8_t>&& userData) {

    if (!m_ltpUdpEngineManagerPtr->ReadyToForward()) { //in case there's a general error for the manager's udp receive, stop it here
        return false;
    }

    const unsigned int startingCount = m_startingCount.fetch_add(1);
    if ((m_activeSessionNumbersSet.size() + startingCount) > M_BUNDLE_PIPELINE_LIMIT) {
        --m_startingCount;
        LOG_ERROR(subprocess) << "LtpBundleSource::Forward(std::vector<uint8_t>.. too many unacked sessions (exceeds bundle pipeline limit of " << M_BUNDLE_PIPELINE_LIMIT << ").";
        return false;
    }

    //TODO? m_ltpUdpEnginePtr->ReadyToForward() if using ping

    std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
    tReq->destinationClientServiceId = M_CLIENT_SERVICE_ID;
    tReq->destinationLtpEngineId = M_REMOTE_LTP_ENGINE_ID; //used for the LtpEngine static singleton session number registrar for tx sessions
    const uint64_t bundleBytesToSend = dataVec.size();
    tReq->clientServiceDataToSend = std::move(dataVec);
    tReq->clientServiceDataToSend.m_userData = std::move(userData);
    tReq->lengthOfRedPart = bundleBytesToSend;

    m_ltpUdpEnginePtr->TransmissionRequest_ThreadSafe(std::move(tReq));

    ++m_ltpOutductTelemetry.totalBundlesSent;
    m_ltpOutductTelemetry.totalBundleBytesSent += bundleBytesToSend;
    
    return true;
}

bool LtpBundleSource::Forward(zmq::message_t & dataZmq, std::vector<uint8_t>&& userData) {

    if (!m_ltpUdpEngineManagerPtr->ReadyToForward()) { //in case there's a general error for the manager's udp receive, stop it here
        return false;
    }

    const unsigned int startingCount = m_startingCount.fetch_add(1);
    if ((m_activeSessionNumbersSet.size() + startingCount) > M_BUNDLE_PIPELINE_LIMIT) {
        --m_startingCount;
        LOG_ERROR(subprocess) << "LtpBundleSource::Forward(zmq::message_t.. too many unacked sessions (exceeds bundle pipeline limit of " << M_BUNDLE_PIPELINE_LIMIT << ").";
        return false;
    }

    std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
    tReq->destinationClientServiceId = M_CLIENT_SERVICE_ID;
    tReq->destinationLtpEngineId = M_REMOTE_LTP_ENGINE_ID; //used for the LtpEngine static singleton session number registrar for tx sessions
    const uint64_t bundleBytesToSend = dataZmq.size();
    tReq->clientServiceDataToSend = std::move(dataZmq);
    tReq->clientServiceDataToSend.m_userData = std::move(userData);
    tReq->lengthOfRedPart = bundleBytesToSend;

    m_ltpUdpEnginePtr->TransmissionRequest_ThreadSafe(std::move(tReq));

    ++m_ltpOutductTelemetry.totalBundlesSent;
    m_ltpOutductTelemetry.totalBundleBytesSent += bundleBytesToSend;
   
    return true;
}

bool LtpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return Forward(vec, std::move(userData));
}



void LtpBundleSource::SessionStartCallback(const Ltp::session_id_t & sessionId) {
    if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
        LOG_ERROR(subprocess) << "LtpBundleSource::SessionStartCallback, sessionOriginatorEngineId "
            << sessionId.sessionOriginatorEngineId << " is not my engine id (" << M_THIS_ENGINE_ID << ")";
    }
    else if (m_activeSessionNumbersSet.insert(sessionId.sessionNumber).second == false) { //sessionId was not inserted (already exists)
        LOG_ERROR(subprocess) << "LtpBundleSource::SessionStartCallback, sessionId " << sessionId << " (already exists)";
    }
    m_startingCount.fetch_sub(1);
}
void LtpBundleSource::TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId) {
    if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
        LOG_ERROR(subprocess) << "LtpBundleSource::TransmissionSessionCompletedCallback, sessionOriginatorEngineId "
            << sessionId.sessionOriginatorEngineId << " is not my engine id (" << M_THIS_ENGINE_ID << ")";
    }
    else if (m_activeSessionNumbersSet.erase(sessionId.sessionNumber)) { //found and erased
        ++m_ltpOutductTelemetry.totalBundlesAcked;
        if (m_useLocalConditionVariableAckReceived) {
            m_localConditionVariableAckReceived.notify_one();
        }
    }
    else {
        LOG_FATAL(subprocess) << "LtpBundleSource::TransmissionSessionCompletedCallback: cannot find sessionId " << sessionId;
    }
}
void LtpBundleSource::InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId) {

}
void LtpBundleSource::TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
    if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
        LOG_ERROR(subprocess) << "LtpBundleSource::TransmissionSessionCancelledCallback, sessionOriginatorEngineId "
            << sessionId.sessionOriginatorEngineId << " is not my engine id (" << M_THIS_ENGINE_ID << ")";
    }
    else if (m_activeSessionNumbersSet.erase(sessionId.sessionNumber)) { //found and erased
        ++m_ltpOutductTelemetry.totalBundlesFailedToSend;
        if (m_useLocalConditionVariableAckReceived) {
            m_localConditionVariableAckReceived.notify_one();
        }
    }
    else {
        LOG_FATAL(subprocess) << "LtpBundleSource::TransmissionSessionCancelledCallback: cannot find sessionId " << sessionId;
    }
}

void LtpBundleSource::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    if (m_ltpUdpEnginePtr) {
        m_ltpUdpEnginePtr->SetOnFailedBundleVecSendCallback(callback);
    }
}
void LtpBundleSource::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    if (m_ltpUdpEnginePtr) {
        m_ltpUdpEnginePtr->SetOnFailedBundleZmqSendCallback(callback);
    }
}
void LtpBundleSource::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    if (m_ltpUdpEnginePtr) {
        m_ltpUdpEnginePtr->SetOnSuccessfulBundleSendCallback(callback);
    }
}
void LtpBundleSource::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    if (m_ltpUdpEnginePtr) {
        m_ltpUdpEnginePtr->SetOnOutductLinkStatusChangedCallback(callback);
    }
}
void LtpBundleSource::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    if (m_ltpUdpEnginePtr) {
        m_ltpUdpEnginePtr->SetUserAssignedUuid(userAssignedUuid);
    }
}

void LtpBundleSource::SyncTelemetry() {
    if (m_ltpUdpEnginePtr) {
        m_ltpOutductTelemetry.numCheckpointsExpired = m_ltpUdpEnginePtr->m_numCheckpointTimerExpiredCallbacks;
        m_ltpOutductTelemetry.numDiscretionaryCheckpointsNotResent = m_ltpUdpEnginePtr->m_numDiscretionaryCheckpointsNotResent;
        m_ltpOutductTelemetry.countUdpPacketsSent = m_ltpUdpEnginePtr->m_countAsyncSendCallbackCalls + m_ltpUdpEnginePtr->m_countBatchUdpPacketsSent;
        m_ltpOutductTelemetry.countRxUdpCircularBufferOverruns = m_ltpUdpEnginePtr->m_countCircularBufferOverruns;
        m_ltpOutductTelemetry.countTxUdpPacketsLimitedByRate = m_ltpUdpEnginePtr->m_countAsyncSendsLimitedByRate;
    }
}
