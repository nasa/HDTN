#include "InductManager.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include "TcpclInduct.h"
#include "StcpInduct.h"
#include "UdpInduct.h"
#include "LtpOverUdpInduct.h"

InductManager::InductManager() {}

InductManager::~InductManager() {}

void InductManager::LoadInductsFromConfig(const InductProcessBundleCallback_t & inductProcessBundleCallback, const InductsConfig & inductsConfig,
    const uint64_t myNodeId, const uint64_t maxUdpRxPacketSizeBytesForAllLtp, const uint64_t maxBundleSizeBytes)
{
    LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(maxUdpRxPacketSizeBytesForAllLtp); //MUST BE CALLED BEFORE ANY USAGE OF LTP
    m_inductsList.clear();
    const induct_element_config_vector_t & configsVec = inductsConfig.m_inductElementConfigVector;
    for (induct_element_config_vector_t::const_iterator it = configsVec.cbegin(); it != configsVec.cend(); ++it) {
        const induct_element_config_t & thisInductConfig = *it;
        if (thisInductConfig.convergenceLayer == "tcpcl") {
            m_inductsList.emplace_back(boost::make_unique<TcpclInduct>(inductProcessBundleCallback, thisInductConfig, myNodeId, maxBundleSizeBytes));
        }
        else if (thisInductConfig.convergenceLayer == "stcp") {
            m_inductsList.emplace_back(boost::make_unique<StcpInduct>(inductProcessBundleCallback, thisInductConfig, maxBundleSizeBytes));
        }
        else if (thisInductConfig.convergenceLayer == "udp") {
            m_inductsList.emplace_back(boost::make_unique<UdpInduct>(inductProcessBundleCallback, thisInductConfig));
        }
        else if (thisInductConfig.convergenceLayer == "ltp_over_udp") {
            m_inductsList.emplace_back(boost::make_unique<LtpOverUdpInduct>(inductProcessBundleCallback, thisInductConfig, maxBundleSizeBytes));
        }
    }
}

void InductManager::Clear() {
    m_inductsList.clear();
}