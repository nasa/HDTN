/**
 * @file router.cpp
 * @author Nadia Kortas <nadia.kortas@nasa.gov>
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

#include "router.h"
#include "Uri.h"
#include "SignalHandler.h"
#include <fstream>
#include "message.hpp"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>
#include <memory>
#include "libcgr.h"
#include "Logger.h"
#include "TelemetryDefinitions.h"
#include <boost/make_unique.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <cstdlib>
#include "Environment.h"
#include "JsonSerializable.h"
#include "HdtnConfig.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::router;

boost::filesystem::path Router::GetFullyQualifiedFilename(const boost::filesystem::path& filename) {
    return (Environment::GetPathHdtnSourceRoot() / "module/router/contact_plans/") / filename;
}

Router::Router() : 
    m_running(false), 
    m_computedInitialOptimalRoutes(false), 
    m_latestTime(0), 
    m_usingUnixTimestamp(false), 
    m_usingMGR(false) {}

Router::~Router() {
    Stop();
}

void Router::Stop() {
    m_running = false;
}

bool Router::Init(uint64_t sourceNodeId,
    const boost::filesystem::path& contactPlanFilePath,
    bool usingUnixTimestamp,
    bool useMgr)
{
    if (m_running) {
        LOG_ERROR(subprocess) << "Router::Init called while Router is already running";
        return false;
    }

    m_sourceNodeId = sourceNodeId;
    m_contactPlanFilePath = contactPlanFilePath;
    m_usingUnixTimestamp = usingUnixTimestamp;
    m_usingMGR = useMgr;

    m_computedInitialOptimalRoutes = false;
    
    m_running = true;

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(subprocess) << "Router up and running at " << timeLocal;
    return true;
}

void Router::HandleLinkDownEvent(const hdtn::IreleaseChangeHdr &releaseChangeHdr) {
    m_latestTime = releaseChangeHdr.time;
    LOG_INFO(subprocess) << "Received Link Down for contact: src = "
        << releaseChangeHdr.prevHopNodeId
        << "  dest = " << releaseChangeHdr.nextHopNodeId
        << "  outductIndex = " << releaseChangeHdr.outductArrayIndex;

    if (!m_computedInitialOptimalRoutes) {
        LOG_ERROR(subprocess) << "out of order command, received link down before receiving outduct capabilities";
        return;
    }

    ComputeOptimalRoutesForOutductIndex(releaseChangeHdr.prevHopNodeId, releaseChangeHdr.outductArrayIndex);
    LOG_DEBUG(subprocess) << "Updated time to " << m_latestTime;
}

void Router::HandleLinkUpEvent(const hdtn::IreleaseChangeHdr &releaseChangeHdr) {
    m_latestTime = releaseChangeHdr.time;
    LOG_DEBUG(subprocess) << "Contact up ";
    LOG_DEBUG(subprocess) << "Updated time to " << m_latestTime;
}

void Router::HandleOutductCapabilitiesTelemetry(const hdtn::IreleaseChangeHdr &releaseChangeHdr, const AllOutductCapabilitiesTelemetry_t & aoct) {
    m_latestTime = releaseChangeHdr.time;
    LOG_INFO(subprocess) << "Received and successfully decoded " << ((m_computedInitialOptimalRoutes) ? "UPDATED" : "NEW")
        << " AllOutductCapabilitiesTelemetry_t message from Scheduler containing "
        << aoct.outductCapabilityTelemetryList.size() << " outducts.";

    m_mapOutductArrayIndexToNextHopPlusFinalDestNodeIdList.clear();

    for (std::list<OutductCapabilityTelemetry_t>::const_iterator itAoct = aoct.outductCapabilityTelemetryList.cbegin();
        itAoct != aoct.outductCapabilityTelemetryList.cend(); ++itAoct)
    {
        const OutductCapabilityTelemetry_t& oct = *itAoct;
        nexthop_finaldestlist_pair_t& nhFdPair = m_mapOutductArrayIndexToNextHopPlusFinalDestNodeIdList[oct.outductArrayIndex];
        nhFdPair.first = oct.nextHopNodeId;
        std::list<uint64_t>& finalDestNodeIdList = nhFdPair.second;
        for (std::list<uint64_t>::const_iterator it = oct.finalDestinationNodeIdList.cbegin();
            it != oct.finalDestinationNodeIdList.cend(); ++it)
        {
            const uint64_t nodeId = *it;
            finalDestNodeIdList.emplace_back(nodeId);
            LOG_INFO(subprocess) << "Compute Optimal Route for finalDestination node " << nodeId;
        }
        std::set<uint64_t> usedNodeIds;
        for (std::list<cbhe_eid_t>::const_iterator it = oct.finalDestinationEidList.cbegin();
            it != oct.finalDestinationEidList.cend(); ++it)
        {
            const uint64_t nodeId = it->nodeId;
            if (usedNodeIds.emplace(nodeId).second) { //was inserted
                finalDestNodeIdList.emplace_back(nodeId);
                LOG_INFO(subprocess) << "Compute Optimal Route for finalDestination node " << nodeId;
            }
        }
        if (!m_computedInitialOptimalRoutes) {
            ComputeOptimalRoutesForOutductIndex(m_sourceNodeId, oct.outductArrayIndex);
        }
    }
    m_computedInitialOptimalRoutes = true;

}

void Router::HandleBundleFromScheduler(const hdtn::IreleaseChangeHdr &releaseChangeHdr) {
    m_latestTime = releaseChangeHdr.time;
}

bool Router::ComputeOptimalRoutesForOutductIndex(uint64_t sourceNode, uint64_t outductIndex) {
    std::map<uint64_t, nexthop_finaldestlist_pair_t>::const_iterator mapIt = m_mapOutductArrayIndexToNextHopPlusFinalDestNodeIdList.find(outductIndex);
    if (mapIt == m_mapOutductArrayIndexToNextHopPlusFinalDestNodeIdList.cend()) {
        LOG_ERROR(subprocess) << "ComputeOptimalRoutesForOutductIndex cannot find outductIndex " << outductIndex;
        return false;
    }

    const nexthop_finaldestlist_pair_t& nhFdPair = mapIt->second;
    const uint64_t originalNextHopNodeId = nhFdPair.first;
    const std::list<uint64_t>& finalDestNodeIdList = nhFdPair.second;
    bool noErrors = true;
    for (std::list<uint64_t>::const_iterator it = finalDestNodeIdList.cbegin();
        it != finalDestNodeIdList.cend(); ++it)
    {
        const uint64_t finalDestNodeId = *it;
        LOG_DEBUG(subprocess) << "FinalDest nodeId found is:  " << finalDestNodeId;
        if (!ComputeOptimalRoute(sourceNode, originalNextHopNodeId, finalDestNodeId)) {
            noErrors = false;
        }
    }
    return noErrors;
}

bool Router::ComputeOptimalRoute(uint64_t sourceNode, uint64_t originalNextHopNodeId, uint64_t finalDestNodeId) {

    cgr::Route bestRoute;

    LOG_DEBUG(subprocess) << "Reading contact plan and computing next hop";
    std::vector<cgr::Contact> contactPlan = cgr::cp_load(m_contactPlanFilePath);

    cgr::Contact rootContact = cgr::Contact(sourceNode,
        sourceNode, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
    rootContact.arrival_time = m_latestTime;
    if (!m_usingMGR) {
        LOG_INFO(subprocess) << "Computing Optimal Route using CGR dijkstra for final Destination "
            << finalDestNodeId << " at latest time " << rootContact.arrival_time;
        bestRoute = cgr::dijkstra(&rootContact, finalDestNodeId, contactPlan);
    }
    else {
        bestRoute = cgr::cmr_dijkstra(&rootContact,
            finalDestNodeId, contactPlan);
        LOG_INFO(subprocess) << "Computing Optimal Route using CMR algorithm for final Destination "
            << finalDestNodeId << " at latest time " << rootContact.arrival_time;
    }

    if (bestRoute.valid()) { // successfully computed a route
        const uint64_t nextHopNodeId = bestRoute.next_node;
	if (originalNextHopNodeId != nextHopNodeId) {
            LOG_INFO(subprocess) << "Successfully Computed next hop: "
                << nextHopNodeId << " for final Destination " << finalDestNodeId
                << "Best route is " << bestRoute;
            if(m_sendRouteUpdate) {
                m_sendRouteUpdate(nextHopNodeId, finalDestNodeId);
            } else {
                LOG_ERROR(subprocess) << "SendRouteUpdate callback not set";
            }

        }
        else {
            LOG_DEBUG(subprocess) << "Skipping Computed next hop: "
                << nextHopNodeId << " for final Destination " << finalDestNodeId
                << " because the next hops didn't change.";
        }
   } else {
        // what should we do if no route is found?
        LOG_ERROR(subprocess) << "No route is found!!";
        return false;
    }

    return true;
}
