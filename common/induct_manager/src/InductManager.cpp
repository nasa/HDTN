/**
 * @file InductManager.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "InductManager.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <memory>
#include "TcpclInduct.h"
#include "TcpclV4Induct.h"
#include "StcpInduct.h"
#include "UdpInduct.h"
#include "LtpOverUdpInduct.h"
#include "LtpOverIpcInduct.h"
#include "LtpOverEncapLocalStreamInduct.h"
#include "BpOverEncapLocalStreamInduct.h"
#include "SlipOverUartInduct.h"
#include "TimestampUtil.h"

InductManager::InductManager() {}

InductManager::~InductManager() {}

bool InductManager::LoadInductsFromConfig(const InductProcessBundleCallback_t & inductProcessBundleCallback, const InductsConfig & inductsConfig,
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
                LOG_ERROR(hdtn::Logger::SubProcess::none) << "error in InductManager::LoadInductsFromConfig: TLS is required for this tcpcl v4 induct but HDTN is not compiled with OpenSSL support.. this induct shall be disabled.";
                continue;
            }
#endif
            m_inductsList.emplace_back(boost::make_unique<TcpclV4Induct>(inductProcessBundleCallback, thisInductConfig,
                myNodeId, maxBundleSizeBytes, onNewOpportunisticLinkCallback, onDeletedOpportunisticLinkCallback));
        }
        else if (thisInductConfig.convergenceLayer == "slip_over_uart") {
            m_inductsList.emplace_back(boost::make_unique<SlipOverUartInduct>(inductProcessBundleCallback, thisInductConfig,
                maxBundleSizeBytes, onNewOpportunisticLinkCallback, onDeletedOpportunisticLinkCallback));
        }
        else if (thisInductConfig.convergenceLayer == "bp_over_encap_local_stream") {
            m_inductsList.emplace_back(boost::make_unique<BpOverEncapLocalStreamInduct>(inductProcessBundleCallback, thisInductConfig,
                maxBundleSizeBytes, onNewOpportunisticLinkCallback, onDeletedOpportunisticLinkCallback));
        }
        else if (thisInductConfig.convergenceLayer == "stcp") {
            m_inductsList.emplace_back(boost::make_unique<StcpInduct>(inductProcessBundleCallback, thisInductConfig,
                maxBundleSizeBytes, onNewOpportunisticLinkCallback, onDeletedOpportunisticLinkCallback));
        }
        else if (thisInductConfig.convergenceLayer == "udp") {
            m_inductsList.emplace_back(boost::make_unique<UdpInduct>(inductProcessBundleCallback, thisInductConfig));
        }
        else if (thisInductConfig.convergenceLayer == "ltp_over_udp") {
            m_inductsList.emplace_back(boost::make_unique<LtpOverUdpInduct>(inductProcessBundleCallback, thisInductConfig, maxBundleSizeBytes));
        }
        else if (thisInductConfig.convergenceLayer == "ltp_over_ipc") {
            m_inductsList.emplace_back(boost::make_unique<LtpOverIpcInduct>(inductProcessBundleCallback, thisInductConfig, maxBundleSizeBytes));
        }
        else if (thisInductConfig.convergenceLayer == "ltp_over_encap_local_stream") {
            m_inductsList.emplace_back(boost::make_unique<LtpOverEncapLocalStreamInduct>(inductProcessBundleCallback, thisInductConfig, maxBundleSizeBytes));
        }
        else {
            LOG_ERROR(hdtn::Logger::SubProcess::none) << "error in InductManager::LoadInductsFromConfig: unknown convergence layer "
                << thisInductConfig.convergenceLayer << " ..skipping";
            continue;
        }

        if (!m_inductsList.back()->Init()) {
            LOG_FATAL(hdtn::Logger::SubProcess::none) << "error in InductManager::LoadInductsFromConfig: unable to initialize";
            return false;
        }
    }
    return true;
}

void InductManager::Clear() {
    m_inductsList.clear();
}

void InductManager::PopulateAllInductTelemetry(AllInductTelemetry_t& allInductTelem) {
    allInductTelem.m_listAllInducts.clear();
    for (std::list<std::unique_ptr<Induct> >::const_iterator it = m_inductsList.cbegin(); it != m_inductsList.cend(); ++it) {
        allInductTelem.m_listAllInducts.emplace_back(); 
        (*it)->PopulateInductTelemetry(allInductTelem.m_listAllInducts.back());
    }
    allInductTelem.m_timestampMilliseconds = TimestampUtil::GetMillisecondsSinceEpochRfc5050();
}
