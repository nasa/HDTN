#include "InductManager.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include "TcpclInduct.h"
#include "TcpclV4Induct.h"
#include "StcpInduct.h"
#include "UdpInduct.h"
#include "LtpOverUdpInduct.h"

InductManager::InductManager() {}

InductManager::~InductManager() {}

void InductManager::LoadInductsFromConfig(const InductProcessBundleCallback_t & inductProcessBundleCallback, const InductsConfig & inductsConfig,
    const uint64_t myNodeId, const uint64_t maxUdpRxPacketSizeBytesForAllLtp, const uint64_t maxBundleSizeBytes,
    const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback, const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback)
{
    LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(maxUdpRxPacketSizeBytesForAllLtp); //MUST BE CALLED BEFORE ANY USAGE OF LTP
    m_inductsList.clear();
    const induct_element_config_vector_t & configsVec = inductsConfig.m_inductElementConfigVector;
    for (induct_element_config_vector_t::const_iterator it = configsVec.cbegin(); it != configsVec.cend(); ++it) {
        const induct_element_config_t & thisInductConfig = *it;
        if (thisInductConfig.convergenceLayer == "tcpcl_v3") {
            m_inductsList.emplace_back(boost::make_unique<TcpclInduct>(inductProcessBundleCallback, thisInductConfig,
                myNodeId, maxBundleSizeBytes, onNewOpportunisticLinkCallback, onDeletedOpportunisticLinkCallback));
        }
        else if (thisInductConfig.convergenceLayer == "tcpcl_v4") {
#ifndef OPENSSL_SUPPORT_ENABLED
            if (thisInductConfig.tlsIsRequired) {
                std::cout << "error in InductManager::LoadInductsFromConfig: TLS is required for this tcpcl v4 induct but HDTN is not compiled with OpenSSL support.. this induct shall be disabled." << std::endl;
                continue;
            }
#endif
            m_inductsList.emplace_back(boost::make_unique<TcpclV4Induct>(inductProcessBundleCallback, thisInductConfig,
                myNodeId, maxBundleSizeBytes, onNewOpportunisticLinkCallback, onDeletedOpportunisticLinkCallback));
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
