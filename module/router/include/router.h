/**
 * @file router.h
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
 *
 * @section DESCRIPTION
 *
 * Router - sends events to egress on the optimal route and next hop.
 */

#ifndef ROUTER_H
#define ROUTER_H 1

#include <cstdint>
#include "zmq.hpp"
#include <memory>
#include "HdtnConfig.h"
#include "HdtnDistributedConfig.h"
#include <boost/core/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include "router_lib_export.h"
#include "message.hpp"
#include "TelemetryDefinitions.h"

class Router : private boost::noncopyable {
public:
    ROUTER_LIB_EXPORT Router();
    ROUTER_LIB_EXPORT ~Router();
    ROUTER_LIB_EXPORT void Stop();
    ROUTER_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const HdtnDistributedConfig& hdtnDistributedConfig,
        const boost::filesystem::path& contactPlanFilePath,
        bool usingUnixTimestamp,
        bool useMgr,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr = NULL);

    ROUTER_LIB_EXPORT static boost::filesystem::path GetFullyQualifiedFilename(const boost::filesystem::path& filename);

    void HandleLinkDownEvent(const hdtn::IreleaseChangeHdr &releaseChangeHdr);
    void HandleLinkUpEvent(const hdtn::IreleaseChangeHdr &releaseChangeHdr);
    void HandleOutductCapabilitiesTelemetry(const hdtn::IreleaseChangeHdr &releaseChangeHdr, const AllOutductCapabilitiesTelemetry_t & aoct);
    void HandleBundleFromScheduler(const hdtn::IreleaseChangeHdr &releaseChangeHdr);

private:
    bool ComputeOptimalRoute(uint64_t sourceNode, uint64_t originalNextHopNodeId, uint64_t finalDestNodeId);
    bool ComputeOptimalRoutesForOutductIndex(uint64_t sourceNode, uint64_t outductIndex);

    void SendRouteUpdate(uint64_t nextHopNodeId, uint64_t finalDestNodeId);

    HdtnConfig m_hdtnConfig;
    volatile bool m_running;
    bool m_usingUnixTimestamp;
    bool m_usingMGR;
    bool m_computedInitialOptimalRoutes;
    uint64_t m_latestTime;
    boost::filesystem::path m_contactPlanFilePath;

    std::unique_ptr<zmq::context_t> m_zmqContextPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingRouterToBoundEgressPtr;

    typedef std::pair<uint64_t, std::list<uint64_t> > nexthop_finaldestlist_pair_t;
    std::map<uint64_t, nexthop_finaldestlist_pair_t> m_mapOutductArrayIndexToNextHopPlusFinalDestNodeIdList;
};


#endif // ROUTER_H
