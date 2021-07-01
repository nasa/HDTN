#include <string>
#include <iostream>
#include "LtpBundleSource.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>

LtpBundleSource::LtpBundleSource(const uint64_t clientServiceId, const uint64_t remoteLtpEngineId, const uint64_t thisEngineId, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const uint16_t udpPort, const bool senderRequireRemoteEndpointMatchOnReceivePacket, const unsigned int numUdpRxCircularBufferVectors,
    const unsigned int maxUdpRxPacketSizeBytes, const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION,
    uint32_t checkpointEveryNthDataPacketSender, uint32_t ltpMaxRetriesPerSerialNumber, const bool force32BitRandomNumbers) :

m_useLocalConditionVariableAckReceived(false), //for destructor only
m_ltpUdpEngine(thisEngineId, mtuClientServiceData, mtuReportSegment, oneWayLightTime, oneWayMarginTime,
    udpPort, false, senderRequireRemoteEndpointMatchOnReceivePacket, numUdpRxCircularBufferVectors, maxUdpRxPacketSizeBytes, ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION,
    checkpointEveryNthDataPacketSender, ltpMaxRetriesPerSerialNumber, force32BitRandomNumbers),

M_CLIENT_SERVICE_ID(clientServiceId),
M_REMOTE_LTP_ENGINE_ID(remoteLtpEngineId),

m_totalDataSegmentsSentSuccessfullyWithAck(0),
m_totalDataSegmentsFailedToSend(0),
m_totalDataSegmentsSent(0),
m_totalBundleBytesSent(0)
{
    m_ltpUdpEngine.SetSessionStartCallback(boost::bind(&LtpBundleSource::SessionStartCallback, this, boost::placeholders::_1));
    m_ltpUdpEngine.SetTransmissionSessionCompletedCallback(boost::bind(&LtpBundleSource::TransmissionSessionCompletedCallback, this, boost::placeholders::_1));
    m_ltpUdpEngine.SetInitialTransmissionCompletedCallback(boost::bind(&LtpBundleSource::InitialTransmissionCompletedCallback, this, boost::placeholders::_1));
    m_ltpUdpEngine.SetTransmissionSessionCancelledCallback(boost::bind(&LtpBundleSource::TransmissionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));
}

LtpBundleSource::~LtpBundleSource() {
    Stop();
}

void LtpBundleSource::Stop() {
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

    //print stats
    std::cout << "m_totalDataSegmentsSentSuccessfullyWithAck " << m_totalDataSegmentsSentSuccessfullyWithAck << std::endl;
    std::cout << "m_totalDataSegmentsFailedToSend " << m_totalDataSegmentsFailedToSend << std::endl;
    std::cout << "m_totalDataSegmentsSent " << m_totalDataSegmentsSent << std::endl;
    std::cout << "m_totalBundleBytesSent " << m_totalBundleBytesSent << std::endl;
}

std::size_t LtpBundleSource::GetTotalDataSegmentsAcked() {
    return m_totalDataSegmentsSentSuccessfullyWithAck + m_totalDataSegmentsFailedToSend;
}

std::size_t LtpBundleSource::GetTotalDataSegmentsSent() {
    return m_totalDataSegmentsSent;
}

std::size_t LtpBundleSource::GetTotalDataSegmentsUnacked() {
    return GetTotalDataSegmentsSent() - GetTotalDataSegmentsAcked();
}

//std::size_t LtpBundleSource::GetTotalBundleBytesAcked() {
//    return m_totalBytesAcked;
//}

std::size_t LtpBundleSource::GetTotalBundleBytesSent() {
    return m_totalBundleBytesSent;
}

//std::size_t LtpBundleSource::GetTotalBundleBytesUnacked() {
//    return GetTotalBundleBytesSent() - GetTotalBundleBytesAcked();
//}

bool LtpBundleSource::Forward(std::vector<uint8_t> & dataVec) {

    if(!m_ltpUdpEngine.ReadyToForward()) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }
    
    
    if (m_activeSessionsSet.size() > 100) {
        std::cerr << "Error in LtpBundleSource::Forward.. too many unacked sessions" << std::endl;
        return false;
    }

    boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
    tReq->destinationClientServiceId = M_CLIENT_SERVICE_ID;
    tReq->destinationLtpEngineId = M_REMOTE_LTP_ENGINE_ID; //TODO THIS ISN'T CURRENTLY USED
    const uint64_t bundleBytesToSend = dataVec.size();
    tReq->clientServiceDataToSend = std::move(dataVec);
    tReq->lengthOfRedPart = bundleBytesToSend;

    m_ltpUdpEngine.TransmissionRequest_ThreadSafe(std::move(tReq));

    ++m_totalDataSegmentsSent;
    m_totalBundleBytesSent += bundleBytesToSend;
    
    return true;
}

bool LtpBundleSource::Forward(zmq::message_t & dataZmq) {

    if (!m_ltpUdpEngine.ReadyToForward()) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }


    if (m_activeSessionsSet.size() > 100) {
        std::cerr << "Error in LtpBundleSource::Forward.. too many unacked sessions" << std::endl;
        return false;
    }

    boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
    tReq->destinationClientServiceId = M_CLIENT_SERVICE_ID;
    tReq->destinationLtpEngineId = M_REMOTE_LTP_ENGINE_ID;
    const uint64_t bundleBytesToSend = dataZmq.size();
    tReq->clientServiceDataToSend = std::move(dataZmq);
    tReq->lengthOfRedPart = bundleBytesToSend;

    m_ltpUdpEngine.TransmissionRequest_ThreadSafe(std::move(tReq));

    ++m_totalDataSegmentsSent;
    m_totalBundleBytesSent += bundleBytesToSend;
   
    return true;
}

bool LtpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return Forward(vec);
}


void LtpBundleSource::Connect(const std::string & hostname, const std::string & port) {
    m_ltpUdpEngine.Connect(hostname, port);
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
        
        ++m_totalDataSegmentsSentSuccessfullyWithAck;
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

        ++m_totalDataSegmentsFailedToSend;
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

bool LtpBundleSource::ReadyToForward() {
    return m_ltpUdpEngine.ReadyToForward();
}

void LtpBundleSource::SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback) {
    m_onSuccessfulAckCallback = callback;
}
