#include "OutductManager.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include "TcpclOutduct.h"
#include "StcpOutduct.h"
#include "UdpOutduct.h"
#include "LtpOverUdpOutduct.h"

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

bool OutductManager::LoadOutductsFromConfig(const OutductsConfig & outductsConfig) {
    m_flowIdToOutductMap.clear();
    m_outductsVec.clear();
    m_threadCommunicationVec.clear();
    uint64_t nextOutductUuidIndex = 0;
    std::set<uint64_t> usedFlowIdSet;
    const outduct_element_config_vector_t & configsVec = outductsConfig.m_outductElementConfigVector;
    m_threadCommunicationVec.resize(configsVec.size());
    m_outductsVec.resize(configsVec.size());
    for (outduct_element_config_vector_t::const_iterator it = configsVec.cbegin(); it != configsVec.cend(); ++it) {
        const outduct_element_config_t & thisOutductConfig = *it;
        boost::shared_ptr<Outduct> outductSharedPtr;
        const uint64_t uuidIndex = nextOutductUuidIndex++;
        m_threadCommunicationVec[uuidIndex] = boost::make_unique<thread_communication_t>();
        if (thisOutductConfig.convergenceLayer == "tcpcl") {
            outductSharedPtr = boost::make_shared<TcpclOutduct>(thisOutductConfig, uuidIndex);
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
            for (std::set<uint64_t>::const_iterator itFlowId = thisOutductConfig.flowIds.cbegin(); itFlowId != thisOutductConfig.flowIds.cend(); ++itFlowId) {
                const uint64_t flowId = *itFlowId;
                if (usedFlowIdSet.insert(flowId).second == false) { //not inserted, flowId already exists
                    std::cerr << "error in OutductManager::LoadOutductsFromConfig: flowId " << flowId << " is already in use by another outduct." << std::endl;
                    return false;
                }
                m_flowIdToOutductMap[flowId] = outductSharedPtr;
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
    m_flowIdToOutductMap.clear();
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

bool OutductManager::Forward(uint64_t flowId, const uint8_t* bundleData, const std::size_t size) {
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_flowIdToOutductMap.at(flowId)) {
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
        std::cerr << "error in  OutductManager::Forward: flowId " << flowId << " not found." << std::endl;
    }
    return false;
}
bool OutductManager::Forward(uint64_t flowId, zmq::message_t & movableDataZmq) {
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_flowIdToOutductMap.at(flowId)) {
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
        std::cerr << "error in  OutductManager::Forward: flowId " << flowId << " not found." << std::endl;
    }
    return false;
}
bool OutductManager::Forward(uint64_t flowId, std::vector<uint8_t> & movableDataVec) {
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_flowIdToOutductMap.at(flowId)) {
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
        std::cerr << "error in  OutductManager::Forward: flowId " << flowId << " not found." << std::endl;
    }
    return false;
}


bool OutductManager::Forward_Blocking(uint64_t flowId, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds) {
    if (timeoutSeconds == 0) {
        return false;
    }
    const uint64_t loopCount = timeoutSeconds * 4;
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_flowIdToOutductMap.at(flowId)) {
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
        std::cerr << "error in  OutductManager::Forward: flowId " << flowId << " not found." << std::endl;
    }
    return false;
}
bool OutductManager::Forward_Blocking(uint64_t flowId, zmq::message_t & movableDataZmq, const uint32_t timeoutSeconds) {
    if (timeoutSeconds == 0) {
        return false;
    }
    const uint64_t loopCount = timeoutSeconds * 4;
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_flowIdToOutductMap.at(flowId)) {
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
        std::cerr << "error in  OutductManager::Forward: flowId " << flowId << " not found." << std::endl;
    }
    return false;
}
bool OutductManager::Forward_Blocking(uint64_t flowId, std::vector<uint8_t> & movableDataVec, const uint32_t timeoutSeconds) {
    if (timeoutSeconds == 0) {
        return false;
    }
    const uint64_t loopCount = timeoutSeconds * 4;
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_flowIdToOutductMap.at(flowId)) {
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
        std::cerr << "error in  OutductManager::Forward: flowId " << flowId << " not found." << std::endl;
    }
    return false;
}

Outduct * OutductManager::GetOutductByFlowId(const uint64_t flowId) {
    try {
        if (boost::shared_ptr<Outduct> & outductPtr = m_flowIdToOutductMap.at(flowId)) {
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