#include "OutductManager.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include "TcpclOutduct.h"
#include "TcpclV4Outduct.h"
#include "StcpOutduct.h"
#include "UdpOutduct.h"
#include "LtpOverUdpOutduct.h"
#include "Uri.h"

OutductManager::OutductManager() : m_numEventsTooManyUnackedBundles(0) {}

OutductManager::~OutductManager() {
    std::cout << "m_numEventsTooManyUnackedBundles " << m_numEventsTooManyUnackedBundles << std::endl;
}

void OutductManager::OnSuccessfulBundleAck(uint64_t uuidIndex) {
    m_threadCommunicationVec[uuidIndex]->Notify();
    if (m_outductManager_onSuccessfulOutductAckCallback) {
        m_outductManager_onSuccessfulOutductAckCallback(uuidIndex);
    }
}

void OutductManager::SetOutductManagerOnSuccessfulOutductAckCallback(const OutductManager_OnSuccessfulOutductAckCallback_t & callback) {
    m_outductManager_onSuccessfulOutductAckCallback = callback;
}

bool OutductManager::LoadOutductsFromConfig(const OutductsConfig & outductsConfig, const uint64_t myNodeId, const uint64_t maxUdpRxPacketSizeBytesForAllLtp,
    const uint64_t maxOpportunisticRxBundleSizeBytes,
    const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback,
    const OnFailedBundleVecSendCallback_t& outductOnFailedBundleVecSendCallback,
    const OnFailedBundleZmqSendCallback_t& outductOnFailedBundleZmqSendCallback,
    const OnOutductLinkStatusChangedCallback_t& onOutductLinkStatusChangedCallback)
{
    LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(maxUdpRxPacketSizeBytesForAllLtp); //MUST BE CALLED BEFORE ANY USAGE OF LTP
    m_finalDestEidToOutductMap.clear();
    m_nextHopEidToOutductMap.clear();
    m_outductsVec.clear();
    m_threadCommunicationVec.clear();
    uint64_t nextOutductUuidIndex = 0;
    std::set<std::string> usedFinalDestEidSet;
    const outduct_element_config_vector_t & configsVec = outductsConfig.m_outductElementConfigVector;
    m_threadCommunicationVec.reserve(configsVec.size());
    m_outductsVec.reserve(configsVec.size());
    for (outduct_element_config_vector_t::const_iterator it = configsVec.cbegin(); it != configsVec.cend(); ++it) {
        const outduct_element_config_t & thisOutductConfig = *it;
        std::shared_ptr<Outduct> outductSharedPtr;
        const uint64_t uuidIndex = nextOutductUuidIndex;
        if (thisOutductConfig.convergenceLayer == "tcpcl_v3") {
            if (thisOutductConfig.tcpclAllowOpportunisticReceiveBundles) {
                outductSharedPtr = std::make_shared<TcpclOutduct>(thisOutductConfig, myNodeId, uuidIndex, outductOpportunisticProcessReceivedBundleCallback);
            }
            else {
                outductSharedPtr = std::make_shared<TcpclOutduct>(thisOutductConfig, myNodeId, uuidIndex);
            }
        }
        else if (thisOutductConfig.convergenceLayer == "tcpcl_v4") {
#ifndef OPENSSL_SUPPORT_ENABLED
            if (thisOutductConfig.tlsIsRequired) {
                std::cout << "error in OutductManager::LoadOutductsFromConfig: TLS is required for this tcpcl v4 outduct but HDTN is not compiled with OpenSSL support.. this outduct shall be disabled." << std::endl;
                continue;
            }
#endif
            if (thisOutductConfig.tcpclAllowOpportunisticReceiveBundles) {
                outductSharedPtr = std::make_shared<TcpclV4Outduct>(thisOutductConfig, myNodeId, uuidIndex, maxOpportunisticRxBundleSizeBytes, outductOpportunisticProcessReceivedBundleCallback);
            }
            else {
                outductSharedPtr = std::make_shared<TcpclV4Outduct>(thisOutductConfig, myNodeId, uuidIndex, maxOpportunisticRxBundleSizeBytes);
            }
        }
        else if (thisOutductConfig.convergenceLayer == "stcp") {
            outductSharedPtr = std::make_shared<StcpOutduct>(thisOutductConfig, uuidIndex);
        }
        else if (thisOutductConfig.convergenceLayer == "udp") {
            outductSharedPtr = std::make_shared<UdpOutduct>(thisOutductConfig, uuidIndex);
        }
        else if (thisOutductConfig.convergenceLayer == "ltp_over_udp") {
            outductSharedPtr = std::make_shared<LtpOverUdpOutduct>(thisOutductConfig, uuidIndex);
        }

        if (outductSharedPtr) {
            ++nextOutductUuidIndex;
            m_threadCommunicationVec.push_back(std::move(boost::make_unique<thread_communication_t>())); //uuid will be the array index
            for (std::set<std::string>::const_iterator itDestUri = thisOutductConfig.finalDestinationEidUris.cbegin(); itDestUri != thisOutductConfig.finalDestinationEidUris.cend(); ++itDestUri) {
                const std::string & finalDestinationEidUri = *itDestUri;
                if (usedFinalDestEidSet.insert(finalDestinationEidUri).second == false) { //not inserted, flowId already exists
                    std::cerr << "error in OutductManager::LoadOutductsFromConfig: finalDestinationEidUri " << finalDestinationEidUri << " is already in use by another outduct." << std::endl;
                    return false;
                }
                cbhe_eid_t destEid;
                if (!Uri::ParseIpnUriString(finalDestinationEidUri, destEid.nodeId, destEid.serviceId)) {
                    std::cerr << "error in OutductManager::LoadOutductsFromConfig: finalDestinationEidUri " << finalDestinationEidUri << " is invalid." << std::endl;
                    return false;
                }
                m_finalDestEidToOutductMap[destEid] = outductSharedPtr;
            }
            cbhe_eid_t nextHopEid;
            if (!Uri::ParseIpnUriString(thisOutductConfig.nextHopEndpointId, nextHopEid.nodeId, nextHopEid.serviceId)) {
                std::cerr << "error in OutductManager::LoadOutductsFromConfig: nextHopEndpointId " <<
                    thisOutductConfig.nextHopEndpointId << " is invalid." << std::endl;
                return false;
            }

            m_nextHopEidToOutductMap[nextHopEid] = outductSharedPtr;

            outductSharedPtr->SetOnSuccessfulAckCallback(boost::bind(&OutductManager::OnSuccessfulBundleAck, this, uuidIndex));
            outductSharedPtr->SetOnFailedBundleVecSendCallback(outductOnFailedBundleVecSendCallback);
            outductSharedPtr->SetOnFailedBundleZmqSendCallback(outductOnFailedBundleZmqSendCallback);
            outductSharedPtr->SetOnOutductLinkStatusChangedCallback(onOutductLinkStatusChangedCallback);
            outductSharedPtr->SetUserAssignedUuid(uuidIndex);
            outductSharedPtr->Connect();
            m_outductsVec.push_back(std::move(outductSharedPtr)); //uuid will be the array index
        }
        else {
            std::cerr << "error in OutductManager::LoadOutductsFromConfig: convergence layer " << thisOutductConfig.convergenceLayer << " is invalid." << std::endl;
            return false;
        }
    }
    return true;
}

void OutductManager::Clear() {
    m_finalDestEidToOutductMap.clear();
    m_nextHopEidToOutductMap.clear();
}

bool OutductManager::AllReadyToForward() const {
    for (std::vector<std::shared_ptr<Outduct> >::const_iterator it = m_outductsVec.cbegin(); it != m_outductsVec.cend(); ++it) {
        const std::shared_ptr<Outduct> & outductPtr = *it;
        if (!outductPtr->ReadyToForward()) {
            return false;
        }
    }
    return true;
}

void OutductManager::StopAllOutducts() {
    for (std::vector<std::shared_ptr<Outduct> >::const_iterator it = m_outductsVec.cbegin(); it != m_outductsVec.cend(); ++it) {
        const std::shared_ptr<Outduct> & outductPtr = *it;
        outductPtr->Stop();
    }
}

bool OutductManager::Forward(const cbhe_eid_t & finalDestEid, const uint8_t* bundleData, const std::size_t size) {
    if (Outduct * const outductPtr = GetOutductByFinalDestinationEid_ThreadSafe(finalDestEid)) {
        if (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
            std::cerr << "bundle pipeline limit exceeded" << std::endl;
            return false;
        }
        else {
            return outductPtr->Forward(bundleData, size);
        }
    }
    else {
        std::cerr << "error in  OutductManager::Forward: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
        return false;
    }
}
bool OutductManager::Forward(const cbhe_eid_t & finalDestEid, zmq::message_t & movableDataZmq) {
    if (Outduct * const outductPtr = GetOutductByFinalDestinationEid_ThreadSafe(finalDestEid)) {
        if (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
            std::cerr << "bundle pipeline limit exceeded" << std::endl;
            return false;
        }
        else {
            return outductPtr->Forward(movableDataZmq);
        }
    }
    else {
        std::cerr << "error in  OutductManager::Forward: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
        return false;
    }
}
bool OutductManager::Forward(const cbhe_eid_t & finalDestEid, std::vector<uint8_t> & movableDataVec) {
    if (Outduct * const outductPtr = GetOutductByFinalDestinationEid_ThreadSafe(finalDestEid)) {
        if (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
            std::cerr << "bundle pipeline limit exceeded" << std::endl;
            return false;
        }
        else {
            return outductPtr->Forward(movableDataVec);
        }
    }
    else {
        std::cerr << "error in  OutductManager::Forward: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
        return false;
    }
}


bool OutductManager::Forward_Blocking(const cbhe_eid_t & finalDestEid, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds) {
    boost::posix_time::ptime timeoutExpiry(boost::posix_time::special_values::not_a_date_time);
    if (Outduct * const outductPtr = GetOutductByFinalDestinationEid_ThreadSafe(finalDestEid)) {
        while (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
            const boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
            if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
                timeoutExpiry = nowTime + boost::posix_time::seconds(timeoutSeconds);
            }
            if (timeoutExpiry <= nowTime) {
                return false;
            }
            m_threadCommunicationVec[outductPtr->GetOutductUuid()]->Wait250msOrUntilNotified();
            ++m_numEventsTooManyUnackedBundles;
        }
        return outductPtr->Forward(bundleData, size);
    }
    else {
        std::cerr << "error in  OutductManager::Forward: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
        return false;
    }
}
bool OutductManager::Forward_Blocking(const cbhe_eid_t & finalDestEid, zmq::message_t & movableDataZmq, const uint32_t timeoutSeconds) {
    boost::posix_time::ptime timeoutExpiry(boost::posix_time::special_values::not_a_date_time);
    if (Outduct * const outductPtr = GetOutductByFinalDestinationEid_ThreadSafe(finalDestEid)) {
        while (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
            const boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
            if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
                timeoutExpiry = nowTime + boost::posix_time::seconds(timeoutSeconds);
            }
            if (timeoutExpiry <= nowTime) {
                return false;
            }
            m_threadCommunicationVec[outductPtr->GetOutductUuid()]->Wait250msOrUntilNotified();
            ++m_numEventsTooManyUnackedBundles;
        }
        return outductPtr->Forward(movableDataZmq);
    }
    else {
        std::cerr << "error in  OutductManager::Forward: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
        return false;
    }
}
bool OutductManager::Forward_Blocking(const cbhe_eid_t & finalDestEid, std::vector<uint8_t> & movableDataVec, const uint32_t timeoutSeconds) {
    boost::posix_time::ptime timeoutExpiry(boost::posix_time::special_values::not_a_date_time);
    if (Outduct * const outductPtr = GetOutductByFinalDestinationEid_ThreadSafe(finalDestEid)) {
        while (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
            const boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
            if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
                timeoutExpiry = nowTime + boost::posix_time::seconds(timeoutSeconds);
            }
            if (timeoutExpiry <= nowTime) {
                return false;
            }
            m_threadCommunicationVec[outductPtr->GetOutductUuid()]->Wait250msOrUntilNotified();
            ++m_numEventsTooManyUnackedBundles;
        }
        return outductPtr->Forward(movableDataVec);
    }
    else {
        std::cerr << "error in  OutductManager::Forward: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
        return false;
    }
}

void OutductManager::SetOutductForFinalDestinationEid_ThreadSafe(const cbhe_eid_t finalDestEid, std::shared_ptr<Outduct> & outductPtr) {
    m_finalDestEidToOutductMapMutex.lock();
    m_finalDestEidToOutductMap[finalDestEid] = outductPtr;
    m_finalDestEidToOutductMapMutex.unlock();
}

Outduct * OutductManager::GetOutductByFinalDestinationEid_ThreadSafe(const cbhe_eid_t & finalDestEid) {
    m_finalDestEidToOutductMapMutex.lock();
    std::map<cbhe_eid_t, std::shared_ptr<Outduct> >::iterator it = m_finalDestEidToOutductMap.find(finalDestEid);
    Outduct * const retVal = (it != m_finalDestEidToOutductMap.end()) ? it->second.get() : NULL;
    m_finalDestEidToOutductMapMutex.unlock();
    return retVal;
}

Outduct * OutductManager::GetOutductByNextHopEid(const cbhe_eid_t & nextHopEid) {
    try {
        if (std::shared_ptr<Outduct> & outductPtr = m_nextHopEidToOutductMap.at(nextHopEid)) {
            return outductPtr.get();
        }
    }
    catch (const std::out_of_range &) {}
    return NULL;
}

Outduct * OutductManager::GetOutductByOutductUuid(const uint64_t uuid) {
    try {
        if (std::shared_ptr<Outduct> & outductPtr = m_outductsVec.at(uuid)) {
            return outductPtr.get();
        }
    }
    catch (const std::out_of_range &) {}
    
    return NULL;
}

std::shared_ptr<Outduct> OutductManager::GetOutductSharedPtrByOutductUuid(const uint64_t uuid) {
    try {
        if (std::shared_ptr<Outduct> & outductPtr = m_outductsVec.at(uuid)) {
            return outductPtr;
        }
    }
    catch (const std::out_of_range &) {}

    return NULL;
}

uint64_t OutductManager::GetAllOutductTelemetry(uint8_t* serialization, uint64_t bufferSize) const {
    const uint8_t* const serializationBase = serialization;
    for (std::vector<std::shared_ptr<Outduct> >::const_iterator it = m_outductsVec.cbegin(); it != m_outductsVec.cend(); ++it) {
        const std::shared_ptr<Outduct>& o = (*it);
        const uint64_t sizeSerialized = o->GetOutductTelemetry(serialization, bufferSize);
        serialization += sizeSerialized;
        bufferSize -= sizeSerialized;
    }
    return serialization - serializationBase;
}
