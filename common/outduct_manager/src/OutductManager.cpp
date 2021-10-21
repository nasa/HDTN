#include "OutductManager.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include "TcpclOutduct.h"
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
    const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback)
{
    LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(maxUdpRxPacketSizeBytesForAllLtp); //MUST BE CALLED BEFORE ANY USAGE OF LTP
    m_finalDestEidToOutductMap.clear();
    m_outductsVec.clear();
    m_threadCommunicationVec.clear();
    uint64_t nextOutductUuidIndex = 0;
    std::set<std::string> usedFinalDestEidSet;
    const outduct_element_config_vector_t & configsVec = outductsConfig.m_outductElementConfigVector;
    m_threadCommunicationVec.resize(configsVec.size());
    m_outductsVec.resize(configsVec.size());
    for (outduct_element_config_vector_t::const_iterator it = configsVec.cbegin(); it != configsVec.cend(); ++it) {
        const outduct_element_config_t & thisOutductConfig = *it;
        boost::shared_ptr<Outduct> outductSharedPtr;
        const uint64_t uuidIndex = nextOutductUuidIndex++;
        m_threadCommunicationVec[uuidIndex] = boost::make_unique<thread_communication_t>();
        if (thisOutductConfig.convergenceLayer == "tcpcl") {
            if (thisOutductConfig.tcpclAllowOpportunisticReceiveBundles) {
                outductSharedPtr = boost::make_shared<TcpclOutduct>(thisOutductConfig, myNodeId, uuidIndex, outductOpportunisticProcessReceivedBundleCallback);
            }
            else {
                outductSharedPtr = boost::make_shared<TcpclOutduct>(thisOutductConfig, myNodeId, uuidIndex);
            }
        }
        else if (thisOutductConfig.convergenceLayer == "stcp") {
            outductSharedPtr = boost::make_shared<StcpOutduct>(thisOutductConfig, uuidIndex);
        }
        else if (thisOutductConfig.convergenceLayer == "udp") {
            outductSharedPtr = boost::make_shared<UdpOutduct>(thisOutductConfig, uuidIndex);
        }
        else if (thisOutductConfig.convergenceLayer == "ltp_over_udp") {
            outductSharedPtr = boost::make_shared<LtpOverUdpOutduct>(thisOutductConfig, uuidIndex);
        }

        if (outductSharedPtr) {
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
            outductSharedPtr->SetOnSuccessfulAckCallback(boost::bind(&OutductManager::OnSuccessfulBundleAck, this, uuidIndex));
            outductSharedPtr->Connect();
            m_outductsVec[uuidIndex] = std::move(outductSharedPtr);
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
}

bool OutductManager::AllReadyToForward() const {
    for (std::vector<boost::shared_ptr<Outduct> >::const_iterator it = m_outductsVec.cbegin(); it != m_outductsVec.cend(); ++it) {
        const boost::shared_ptr<Outduct> & outductPtr = *it;
        if (!outductPtr->ReadyToForward()) {
            return false;
        }
    }
    return true;
}

void OutductManager::StopAllOutducts() {
    for (std::vector<boost::shared_ptr<Outduct> >::const_iterator it = m_outductsVec.cbegin(); it != m_outductsVec.cend(); ++it) {
        const boost::shared_ptr<Outduct> & outductPtr = *it;
        outductPtr->Stop();
    }
}

bool OutductManager::Forward(const cbhe_eid_t & finalDestEid, const uint8_t* bundleData, const std::size_t size) {
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_finalDestEidToOutductMap.at(finalDestEid)) {
            if (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
                std::cerr << "bundle pipeline limit exceeded" << std::endl;
                return false;
            }
            else {
                return outductPtr->Forward(bundleData, size);
            }
        }
    }
    catch (const std::out_of_range &) {
        std::cerr << "error in  OutductManager::Forward: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
    }
    return false;
}
bool OutductManager::Forward(const cbhe_eid_t & finalDestEid, zmq::message_t & movableDataZmq) {
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_finalDestEidToOutductMap.at(finalDestEid)) {
            if (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
                std::cerr << "bundle pipeline limit exceeded" << std::endl;
                return false;
            }
            else {
                return outductPtr->Forward(movableDataZmq);
            }
        }
    }
    catch (const std::out_of_range &) {
        std::cerr << "error in  OutductManager::Forward: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
    }
    return false;
}
bool OutductManager::Forward(const cbhe_eid_t & finalDestEid, std::vector<uint8_t> & movableDataVec) {
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_finalDestEidToOutductMap.at(finalDestEid)) {
            if (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
                std::cerr << "bundle pipeline limit exceeded" << std::endl;
                return false;
            }
            else {
                return outductPtr->Forward(movableDataVec);
            }
        }
    }
    catch (const std::out_of_range &) {
        std::cerr << "error in  OutductManager::Forward: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
    }
    return false;
}


bool OutductManager::Forward_Blocking(const cbhe_eid_t & finalDestEid, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds) {
    if (timeoutSeconds == 0) {
        return false;
    }
    const uint64_t loopCount = timeoutSeconds * 4;
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_finalDestEidToOutductMap.at(finalDestEid)) {
            for (uint64_t i = 0; ; ++i) {
                if (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
                    ++m_numEventsTooManyUnackedBundles;
                    if (i == loopCount) {
                        return false;
                    }
                    m_threadCommunicationVec[outductPtr->GetOutductUuid()]->Wait250msOrUntilNotified();
                }
                else {
                    return outductPtr->Forward(bundleData, size);
                }
            }
        }
    }
    catch (const std::out_of_range &) {
        std::cerr << "error in  OutductManager::Forward_Blocking: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
    }
    return false;
}
bool OutductManager::Forward_Blocking(const cbhe_eid_t & finalDestEid, zmq::message_t & movableDataZmq, const uint32_t timeoutSeconds) {
    if (timeoutSeconds == 0) {
        return false;
    }
    const uint64_t loopCount = timeoutSeconds * 4;
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_finalDestEidToOutductMap.at(finalDestEid)) {
            for (uint64_t i = 0; ; ++i) {
                if (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
                    ++m_numEventsTooManyUnackedBundles;
                    if (i == loopCount) {
                        return false;
                    }
                    m_threadCommunicationVec[outductPtr->GetOutductUuid()]->Wait250msOrUntilNotified();
                }
                else {
                    return outductPtr->Forward(movableDataZmq);
                }
            }
        }
    }
    catch (const std::out_of_range &) {
        std::cerr << "error in  OutductManager::Forward_Blocking: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
    }
    return false;
}
bool OutductManager::Forward_Blocking(const cbhe_eid_t & finalDestEid, std::vector<uint8_t> & movableDataVec, const uint32_t timeoutSeconds) {
    if (timeoutSeconds == 0) {
        return false;
    }
    const uint64_t loopCount = timeoutSeconds * 4;
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_finalDestEidToOutductMap.at(finalDestEid)) {
            for (uint64_t i = 0; ; ++i) {
                if (outductPtr->GetTotalDataSegmentsUnacked() > outductPtr->GetOutductMaxBundlesInPipeline()) {
                    ++m_numEventsTooManyUnackedBundles;
                    if (i == loopCount) {
                        return false;
                    }
                    m_threadCommunicationVec[outductPtr->GetOutductUuid()]->Wait250msOrUntilNotified();
                }
                else {
                    return outductPtr->Forward(movableDataVec);
                }
            }
        }
    }
    catch (const std::out_of_range &) {
        std::cerr << "error in  OutductManager::Forward_Blocking: finalDestEid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ") not found." << std::endl;
    }
    return false;
}

Outduct * OutductManager::GetOutductByFinalDestinationEid(const cbhe_eid_t & finalDestEid) {
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_finalDestEidToOutductMap.at(finalDestEid)) {
            return outductPtr.get();
        }
    }
    catch (const std::out_of_range &) {}

    return NULL;
}
Outduct * OutductManager::GetOutductByOutductUuid(const uint64_t uuid) {
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_outductsVec[uuid]) {
            return outductPtr.get();
        }
    }
    catch (const std::out_of_range &) {}

    return NULL;
}