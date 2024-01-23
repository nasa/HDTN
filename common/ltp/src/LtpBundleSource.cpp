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

m_ltpTxCfg(ltpTxCfg),
m_ltpEnginePtr(NULL),

M_CLIENT_SERVICE_ID(ltpTxCfg.clientServiceId),
M_THIS_ENGINE_ID(ltpTxCfg.thisEngineId),
M_REMOTE_LTP_ENGINE_ID(ltpTxCfg.remoteEngineId),
M_BUNDLE_PIPELINE_LIMIT(ltpTxCfg.maxSimultaneousSessions),
m_startingCount(0),
//telemetry
m_totalBundlesSent(0),
m_totalBundlesAcked(0),
m_totalBundlesFailedToSend(0),
m_totalBundleBytesSent(0)
{
}

LtpBundleSource::~LtpBundleSource() {
    Stop(); //child destructor should call stop and set m_ltpEnginePtr to null, if so this Stop() will do nothing since m_ltpEnginePtr will be null
}

bool LtpBundleSource::Init() {
    m_activeSessionNumbersSet.reserve(M_BUNDLE_PIPELINE_LIMIT);
    m_activeSessionNumbersSet.get_allocator().SetMaxListSizeFromGetAllocatorCopy(M_BUNDLE_PIPELINE_LIMIT + 2);
    if (!SetLtpEnginePtr()) { //virtual function call
        return false;
    }

    m_ltpEnginePtr->SetSessionStartCallback(boost::bind(&LtpBundleSource::SessionStartCallback, this, boost::placeholders::_1));
    m_ltpEnginePtr->SetTransmissionSessionCompletedCallback(boost::bind(&LtpBundleSource::TransmissionSessionCompletedCallback, this, boost::placeholders::_1));
    m_ltpEnginePtr->SetInitialTransmissionCompletedCallback(boost::bind(&LtpBundleSource::InitialTransmissionCompletedCallback, this, boost::placeholders::_1));
    m_ltpEnginePtr->SetTransmissionSessionCancelledCallback(boost::bind(&LtpBundleSource::TransmissionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));
    return true;
}



void LtpBundleSource::Stop() {
    try {
        if (m_ltpEnginePtr) {
            //prevent LtpBundleSource from exiting before all bundles sent and acked
            boost::mutex localMutex;
            boost::mutex::scoped_lock lock(localMutex);
            m_useLocalConditionVariableAckReceived = true;
            std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
            for (unsigned int attempt = 0; attempt < 10; ++attempt) {
                const std::size_t numUnacked = GetTotalBundlesUnacked();
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
            //print stats once
            LOG_INFO(subprocess) << "LTP Outduct / Bundle Source:"
                << "\n totalBundlesSent " << GetTotalBundlesSent()
                << "\n totalBundlesAcked " << m_totalBundlesAcked.load(std::memory_order_acquire)
                << "\n totalBundlesFailedToSend " << m_totalBundlesFailedToSend.load(std::memory_order_acquire)
                << "\n totalBundleBytesSent " << GetTotalBundleBytesSent()
                << "\n totalBundleBytesAcked " << m_ltpEnginePtr->m_totalRedDataBytesSuccessfullySent.load(std::memory_order_acquire)
                << "\n totalBundleBytesFailedToSend " << m_ltpEnginePtr->m_totalRedDataBytesFailedToSend.load(std::memory_order_acquire);
            m_ltpEnginePtr = NULL;
        }
    }
    catch (const boost::condition_error& e) {
        LOG_ERROR(subprocess) << "condition_error in LtpBundleSource::Stop: " << e.what();
    }
    catch (const boost::thread_resource_error& e) {
        LOG_ERROR(subprocess) << "thread_resource_error in LtpBundleSource::Stop: " << e.what();
    }
    catch (const boost::thread_interrupted&) {
        LOG_ERROR(subprocess) << "thread_interrupted in LtpBundleSource::Stop";
    }
    catch (const boost::lock_error& e) {
        LOG_ERROR(subprocess) << "lock_error in LtpBundleSource::Stop: " << e.what();
    }
}

std::size_t LtpBundleSource::GetTotalBundlesAcked() const noexcept {
    return m_totalBundlesAcked.load(std::memory_order_acquire) + m_totalBundlesFailedToSend.load(std::memory_order_acquire);
}
std::size_t LtpBundleSource::GetTotalBundlesSent() const noexcept {
    return m_totalBundlesSent.load(std::memory_order_acquire);
}
std::size_t LtpBundleSource::GetTotalBundlesUnacked() const noexcept {
    return GetTotalBundlesSent() - GetTotalBundlesAcked();
}
std::size_t LtpBundleSource::GetTotalBundleBytesAcked() const noexcept {
    return m_ltpEnginePtr->m_totalRedDataBytesSuccessfullySent.load(std::memory_order_acquire)
        + m_ltpEnginePtr->m_totalRedDataBytesFailedToSend.load(std::memory_order_acquire);
}
std::size_t LtpBundleSource::GetTotalBundleBytesSent() const noexcept {
    return m_totalBundleBytesSent.load(std::memory_order_acquire);
}
std::size_t LtpBundleSource::GetTotalBundleBytesUnacked() const noexcept {
    return GetTotalBundleBytesSent() - GetTotalBundleBytesAcked();
}

bool LtpBundleSource::Forward(padded_vector_uint8_t& dataVec, std::vector<uint8_t>&& userData) {

    if (!ReadyToForward()) { //virtual function call
        return false;
    }

    const unsigned int startingCount = m_startingCount.fetch_add(1);
    if ((m_activeSessionNumbersSet.size() + startingCount) > M_BUNDLE_PIPELINE_LIMIT) {
        --m_startingCount;
        LOG_ERROR(subprocess) << "LtpBundleSource::Forward(padded_vector_uint8_t.. too many unacked sessions (exceeds bundle pipeline limit of " << M_BUNDLE_PIPELINE_LIMIT << ").";
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

    m_ltpEnginePtr->TransmissionRequest_ThreadSafe(std::move(tReq));

    m_totalBundlesSent.fetch_add(1, std::memory_order_relaxed);
    m_totalBundleBytesSent.fetch_add(bundleBytesToSend, std::memory_order_relaxed);
    
    return true;
}

bool LtpBundleSource::Forward(zmq::message_t & dataZmq, std::vector<uint8_t>&& userData) {

    if (!ReadyToForward()) { //virtual function call
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

    m_ltpEnginePtr->TransmissionRequest_ThreadSafe(std::move(tReq));

    m_totalBundlesSent.fetch_add(1, std::memory_order_relaxed);
    m_totalBundleBytesSent.fetch_add(bundleBytesToSend, std::memory_order_relaxed);
   
    return true;
}

bool LtpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    padded_vector_uint8_t vec(bundleData, bundleData + size);
    return Forward(vec, std::move(userData));
}

uint64_t LtpBundleSource::GetOutductMaxNumberOfBundlesInPipeline() const {
    return m_ltpEnginePtr->GetMaxNumberOfSessionsInPipeline();
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
        m_totalBundlesAcked.fetch_add(1, std::memory_order_relaxed);
        if (m_useLocalConditionVariableAckReceived.load(std::memory_order_acquire)) {
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
        m_totalBundlesFailedToSend.fetch_add(1, std::memory_order_relaxed);
        if (m_useLocalConditionVariableAckReceived.load(std::memory_order_acquire)) {
            m_localConditionVariableAckReceived.notify_one();
        }
    }
    else {
        LOG_FATAL(subprocess) << "LtpBundleSource::TransmissionSessionCancelledCallback: cannot find sessionId " << sessionId;
    }
}

void LtpBundleSource::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    if (m_ltpEnginePtr) {
        m_ltpEnginePtr->SetOnFailedBundleVecSendCallback(callback);
    }
}
void LtpBundleSource::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    if (m_ltpEnginePtr) {
        m_ltpEnginePtr->SetOnFailedBundleZmqSendCallback(callback);
    }
}
void LtpBundleSource::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    if (m_ltpEnginePtr) {
        m_ltpEnginePtr->SetOnSuccessfulBundleSendCallback(callback);
    }
}
void LtpBundleSource::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    if (m_ltpEnginePtr) {
        m_ltpEnginePtr->SetOnOutductLinkStatusChangedCallback(callback);
    }
}
void LtpBundleSource::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    if (m_ltpEnginePtr) {
        m_ltpEnginePtr->SetUserAssignedUuid(userAssignedUuid);
    }
}
void LtpBundleSource::SetRate(uint64_t maxSendRateBitsPerSecOrZeroToDisable) {
    if (m_ltpEnginePtr) {
        m_ltpEnginePtr->SetRate_ThreadSafe(maxSendRateBitsPerSecOrZeroToDisable);
    }
}
void LtpBundleSource::SetPing(uint64_t senderPingSecondsOrZeroToDisable) {
    if (m_ltpEnginePtr) {
        m_ltpEnginePtr->SetPing_ThreadSafe(senderPingSecondsOrZeroToDisable);
    }
}
void LtpBundleSource::SetPingToDefaultConfig() {
    if (m_ltpEnginePtr) {
        m_ltpEnginePtr->SetPingToDefaultConfig_ThreadSafe();
    }
}

void LtpBundleSource::GetTelemetry(LtpOutductTelemetry_t& telem) const {
    if (m_ltpEnginePtr) {
        telem.m_totalBundlesSent = m_totalBundlesSent.load(std::memory_order_acquire);
        telem.m_totalBundlesAcked = m_totalBundlesAcked.load(std::memory_order_acquire);
        telem.m_totalBundleBytesSent = m_totalBundleBytesSent.load(std::memory_order_acquire);
        telem.m_totalBundlesFailedToSend = m_totalBundlesFailedToSend.load(std::memory_order_acquire);

        telem.m_numCheckpointsExpired = m_ltpEnginePtr->m_numCheckpointTimerExpiredCallbacksRef.load(std::memory_order_acquire);
        telem.m_numDiscretionaryCheckpointsNotResent = m_ltpEnginePtr->m_numDiscretionaryCheckpointsNotResentRef.load(std::memory_order_acquire);
        telem.m_numDeletedFullyClaimedPendingReports = m_ltpEnginePtr->m_numDeletedFullyClaimedPendingReportsRef.load(std::memory_order_acquire);

        telem.m_totalCancelSegmentsStarted = m_ltpEnginePtr->m_totalCancelSegmentsStarted.load(std::memory_order_acquire);
        telem.m_totalCancelSegmentSendRetries = m_ltpEnginePtr->m_totalCancelSegmentSendRetries.load(std::memory_order_acquire);
        telem.m_totalCancelSegmentsFailedToSend = m_ltpEnginePtr->m_totalCancelSegmentsFailedToSend.load(std::memory_order_acquire);
        telem.m_totalCancelSegmentsAcknowledged = m_ltpEnginePtr->m_totalCancelSegmentsAcknowledged.load(std::memory_order_acquire);
        telem.m_totalPingsStarted = m_ltpEnginePtr->m_totalPingsStarted.load(std::memory_order_acquire);
        telem.m_totalPingRetries = m_ltpEnginePtr->m_totalPingRetries.load(std::memory_order_acquire);
        telem.m_totalPingsFailedToSend = m_ltpEnginePtr->m_totalPingsFailedToSend.load(std::memory_order_acquire);
        telem.m_totalPingsAcknowledged = m_ltpEnginePtr->m_totalPingsAcknowledged.load(std::memory_order_acquire);
        telem.m_numTxSessionsReturnedToStorage = m_ltpEnginePtr->m_numTxSessionsReturnedToStorage.load(std::memory_order_acquire);
        telem.m_numTxSessionsCancelledByReceiver = m_ltpEnginePtr->m_numTxSessionsCancelledByReceiver.load(std::memory_order_acquire);

        telem.m_countTxUdpPacketsLimitedByRate = m_ltpEnginePtr->m_countAsyncSendsLimitedByRate.load(std::memory_order_acquire);
        telem.m_totalBundleBytesAcked = m_ltpEnginePtr->m_totalRedDataBytesSuccessfullySent.load(std::memory_order_acquire);
        //telem.m_totalBundleBytesFailedToSend = m_ltpEnginePtr->m_totalRedDataBytesFailedToSend;
        GetTransportLayerSpecificTelem(telem); //virtual function call
    }
}
