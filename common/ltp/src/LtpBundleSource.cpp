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
#include <iostream>
#include "LtpBundleSource.h"
#include <boost/lexical_cast.hpp>
#include <memory>

LtpBundleSource::LtpBundleSource(const uint64_t clientServiceId, const uint64_t remoteLtpEngineId, const uint64_t thisEngineId, const uint64_t mtuClientServiceData,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const uint16_t myBoundUdpPort, const unsigned int numUdpRxCircularBufferVectors,
    uint32_t checkpointEveryNthDataPacketSender, uint32_t ltpMaxRetriesPerSerialNumber, const bool force32BitRandomNumbers,
    const std::string & remoteUdpHostname, const uint16_t remoteUdpPort, const uint64_t maxSendRateBitsPerSecOrZeroToDisable,
    const uint32_t bundlePipelineLimit, const uint64_t maxUdpPacketsToSendPerSystemCall) :

m_useLocalConditionVariableAckReceived(false), //for destructor only

m_ltpUdpEngineManagerPtr(LtpUdpEngineManager::GetOrCreateInstance(myBoundUdpPort, true)),
M_CLIENT_SERVICE_ID(clientServiceId),
M_THIS_ENGINE_ID(thisEngineId),
M_REMOTE_LTP_ENGINE_ID(remoteLtpEngineId),
M_BUNDLE_PIPELINE_LIMIT(bundlePipelineLimit),

m_ltpOutductTelemetry()
{
    m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(remoteLtpEngineId, false);
    if (m_ltpUdpEnginePtr == NULL) {
        m_ltpUdpEngineManagerPtr->AddLtpUdpEngine(thisEngineId, remoteLtpEngineId, false, mtuClientServiceData, 80, oneWayLightTime, oneWayMarginTime,
            remoteUdpHostname, remoteUdpPort, numUdpRxCircularBufferVectors, 0, 0, 0, ltpMaxRetriesPerSerialNumber,
            force32BitRandomNumbers, maxSendRateBitsPerSecOrZeroToDisable, bundlePipelineLimit, 0, maxUdpPacketsToSendPerSystemCall);
        m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(remoteLtpEngineId, false);
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
    m_removeCallbackCalled = true;
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
                std::cout << "notice: LtpBundleSource destructor waiting on " << numUnacked << " unacked bundles" << std::endl;

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

        m_removeCallbackCalled = false;
        m_ltpUdpEngineManagerPtr->RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(M_REMOTE_LTP_ENGINE_ID, false, boost::bind(&LtpBundleSource::RemoveCallback, this));
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        for (unsigned int attempt = 0; attempt < 20; ++attempt) {
            if (m_removeCallbackCalled) {
                break;
            }
            std::cout << "waiting to remove ltp bundle source for engine ID " << M_THIS_ENGINE_ID << std::endl;
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        }
        m_ltpUdpEnginePtr = NULL;

        //print stats
        std::cout << "m_ltpOutductTelemetry.totalBundlesSent " << m_ltpOutductTelemetry.totalBundlesSent << std::endl;
        std::cout << "m_ltpOutductTelemetry.totalBundlesAcked " << m_ltpOutductTelemetry.totalBundlesAcked << std::endl;
        std::cout << "m_ltpOutductTelemetry.totalBundleBytesSent " << m_ltpOutductTelemetry.totalBundleBytesSent << std::endl;
        std::cout << "m_ltpOutductTelemetry.totalBundleBytesAcked " << m_ltpOutductTelemetry.totalBundleBytesAcked << std::endl;
        std::cout << "m_ltpOutductTelemetry.totalBundlesFailedToSend " << m_ltpOutductTelemetry.totalBundlesFailedToSend << std::endl;
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

bool LtpBundleSource::Forward(std::vector<uint8_t> & dataVec) {
    
    if (m_activeSessionsSet.size() > M_BUNDLE_PIPELINE_LIMIT) {
        std::cerr << "Error in LtpBundleSource::Forward(std::vector<uint8_t>.. too many unacked sessions (exceeds bundle pipeline limit of " << M_BUNDLE_PIPELINE_LIMIT << ")." << std::endl;
        return false;
    }

    std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
    tReq->destinationClientServiceId = M_CLIENT_SERVICE_ID;
    tReq->destinationLtpEngineId = M_REMOTE_LTP_ENGINE_ID; //used for the LtpEngine static singleton session number registrar for tx sessions
    const uint64_t bundleBytesToSend = dataVec.size();
    tReq->clientServiceDataToSend = std::move(dataVec);
    tReq->lengthOfRedPart = bundleBytesToSend;

    m_ltpUdpEnginePtr->TransmissionRequest_ThreadSafe(std::move(tReq));

    ++m_ltpOutductTelemetry.totalBundlesSent;
    m_ltpOutductTelemetry.totalBundleBytesSent += bundleBytesToSend;
    
    return true;
}

bool LtpBundleSource::Forward(zmq::message_t & dataZmq) {


    if (m_activeSessionsSet.size() > M_BUNDLE_PIPELINE_LIMIT) {
        std::cerr << "Error in LtpBundleSource::Forward(zmq::message_t.. too many unacked sessions (exceeds bundle pipeline limit of " << M_BUNDLE_PIPELINE_LIMIT << ")." << std::endl;
        return false;
    }

    std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
    tReq->destinationClientServiceId = M_CLIENT_SERVICE_ID;
    tReq->destinationLtpEngineId = M_REMOTE_LTP_ENGINE_ID; //used for the LtpEngine static singleton session number registrar for tx sessions
    const uint64_t bundleBytesToSend = dataZmq.size();
    tReq->clientServiceDataToSend = std::move(dataZmq);
    tReq->lengthOfRedPart = bundleBytesToSend;

    m_ltpUdpEnginePtr->TransmissionRequest_ThreadSafe(std::move(tReq));

    ++m_ltpOutductTelemetry.totalBundlesSent;
    m_ltpOutductTelemetry.totalBundleBytesSent += bundleBytesToSend;
   
    return true;
}

bool LtpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return Forward(vec);
}



void LtpBundleSource::SessionStartCallback(const Ltp::session_id_t & sessionId) {
    if (m_activeSessionsSet.insert(sessionId).second == false) { //sessionId was not inserted (already exists)
        std::cerr << "error in LtpBundleSource::SessionStartCallback, sessionId " << sessionId << " (already exists)\n";
    }
}
void LtpBundleSource::TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId) {
    std::set<Ltp::session_id_t>::iterator it = m_activeSessionsSet.find(sessionId);
    if (it != m_activeSessionsSet.end()) { //found
        m_activeSessionsSet.erase(it);
        
        ++m_ltpOutductTelemetry.totalBundlesAcked;
        //m_totalBytesAcked += m_bytesToAckCbVec[readIndex];
        //m_bytesToAckCb.CommitRead();
        if (m_onSuccessfulAckCallback) {
            m_onSuccessfulAckCallback();
        }
        if (m_useLocalConditionVariableAckReceived) {
            m_localConditionVariableAckReceived.notify_one();
        }
    }
    else {
        std::cerr << "critical error in LtpBundleSource::TransmissionSessionCompletedCallback: cannot find sessionId " << sessionId << std::endl;
    }
}
void LtpBundleSource::InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId) {

}
void LtpBundleSource::TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
    std::set<Ltp::session_id_t>::iterator it = m_activeSessionsSet.find(sessionId);
    if (it != m_activeSessionsSet.end()) { //found
        m_activeSessionsSet.erase(it);

        ++m_ltpOutductTelemetry.totalBundlesFailedToSend;
        //m_totalBytesAcked += m_bytesToAckCbVec[readIndex];
        //m_bytesToAckCb.CommitRead();
        if (m_onSuccessfulAckCallback) {
            m_onSuccessfulAckCallback();
        }
        if (m_useLocalConditionVariableAckReceived) {
            m_localConditionVariableAckReceived.notify_one();
        }
    }
    else {
        std::cerr << "critical error in LtpBundleSource::TransmissionSessionCancelledCallback: cannot find sessionId " << sessionId << std::endl;
    }
}


void LtpBundleSource::SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback) {
    m_onSuccessfulAckCallback = callback;
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
