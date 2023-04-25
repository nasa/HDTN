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

bool Router::Init(const HdtnConfig& hdtnConfig,
    const HdtnDistributedConfig& hdtnDistributedConfig,
    const boost::filesystem::path& contactPlanFilePath,
    bool usingUnixTimestamp,
    bool useMgr,
    zmq::context_t* hdtnOneProcessZmqInprocContextPtr)
{
    if (m_running) {
        LOG_ERROR(subprocess) << "Router::Init called while Router is already running";
        return false;
    }

    m_hdtnConfig = hdtnConfig;
    m_contactPlanFilePath = contactPlanFilePath;
    m_usingUnixTimestamp = usingUnixTimestamp;
    m_usingMGR = useMgr;

    m_computedInitialOptimalRoutes = false;
    
    // socket for receiving events from scheduler
    m_zmqContextPtr = boost::make_unique<zmq::context_t>();

    try {
        if (hdtnOneProcessZmqInprocContextPtr) {
            //socket for getting Route Update events from Router to Egress
            m_zmqPushSock_connectingRouterToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPushSock_connectingRouterToBoundEgressPtr->connect(std::string("inproc://connecting_router_to_bound_egress"));
        }
        else {
            //socket for getting Route Update events from Router to Egress
            m_zmqPushSock_connectingRouterToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::push);
            const std::string connect_connectingRouterToBoundEgressPath(
                std::string("tcp://") +
                hdtnDistributedConfig.m_zmqEgressAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingRouterToBoundEgressPortPath));
            m_zmqPushSock_connectingRouterToBoundEgressPtr->connect(connect_connectingRouterToBoundEgressPath);
        }

        //Caution: All options, with the exception of ZMQ_SUBSCRIBE, ZMQ_UNSUBSCRIBE and ZMQ_LINGER, only take effect for subsequent socket bind/connects.
        //The value of 0 specifies no linger period. Pending messages shall be discarded immediately when the socket is closed with zmq_close().
        m_zmqPushSock_connectingRouterToBoundEgressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(subprocess) << "egress cannot set up zmq socket: " << ex.what();
        return false;
    }

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

    const uint64_t srcNode = m_hdtnConfig.m_myNodeId;

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
            ComputeOptimalRoutesForOutductIndex(srcNode, oct.outductArrayIndex);
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

void Router::SendRouteUpdate(uint64_t nextHopNodeId, uint64_t finalDestNodeId) {
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    // Timer was not cancelled, take necessary action.
    LOG_INFO(subprocess) << timeLocal << ": Sending RouteUpdate event to Egress ";

    hdtn::RouteUpdateHdr routingMsg;
    memset(&routingMsg, 0, sizeof(hdtn::RouteUpdateHdr));
    routingMsg.base.type = HDTN_MSGTYPE_ROUTEUPDATE;
    routingMsg.nextHopNodeId = nextHopNodeId;
    routingMsg.finalDestNodeId = finalDestNodeId;

    while (m_running && !m_zmqPushSock_connectingRouterToBoundEgressPtr->send(
        zmq::const_buffer(&routingMsg, sizeof(hdtn::RouteUpdateHdr)),
        zmq::send_flags::dontwait))
    {
        //send returns false if(!res.has_value() AND zmq_errno()=EAGAIN)
        LOG_DEBUG(subprocess) << "waiting for egress to become available to send route update";
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
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
            SendRouteUpdate(nextHopNodeId, finalDestNodeId);

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
