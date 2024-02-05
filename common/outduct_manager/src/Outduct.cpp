/**
 * @file Outduct.cpp
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

#include "Outduct.h"
#include "Logger.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

Outduct::Outduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid, const bool assumedInitiallyDown) :
    m_outductConfig(outductConfig),
    m_outductUuid(outductUuid),
    m_assumedInitiallyDown(assumedInitiallyDown),
    m_linkIsUpPerTimeSchedule(false),
    m_physicalLinkStatusIsKnown(false),
    m_linkIsUpPhysically(false) //don't care, set properly when m_physicalLinkStatusIsKnown gets set to true
{}
Outduct::Outduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid) :
    Outduct(outductConfig, outductUuid, true)
{}
Outduct::~Outduct() {}

bool Outduct::Init() { //optional
    return true;
}

uint64_t Outduct::GetOutductUuid() const {
    return m_outductUuid;
}
uint64_t Outduct::GetOutductMaxNumberOfBundlesInPipeline() const {
    return m_outductConfig.maxNumberOfBundlesInPipeline; //virtual method override for LTP (special case for disk sessions)
}
uint64_t Outduct::GetOutductMaxSumOfBundleBytesInPipeline() const {
    return m_outductConfig.maxSumOfBundleBytesInPipeline;
}
uint64_t Outduct::GetOutductNextHopNodeId() const {
    return m_outductConfig.nextHopNodeId;
}
std::string Outduct::GetConvergenceLayerName() const {
    return m_outductConfig.convergenceLayer;
}
bool Outduct::GetAssumedInitiallyDown() const {
    return m_assumedInitiallyDown;
}

void Outduct::SetRate(uint64_t maxSendRateBitsPerSecOrZeroToDisable) {
    if(maxSendRateBitsPerSecOrZeroToDisable) {
        LOG_WARNING(subprocess) <<"outduct " << m_outductUuid << " does not support rate limits";
    }
}
