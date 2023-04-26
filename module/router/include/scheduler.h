/**
 * @file scheduler.h
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
 * This Scheduler class is a process which sends LINKUP or LINKDOWN events to Ingress and Storage modules to determine if a given bundle 
 * should be forwarded immediately to Egress or should be stored. 
 * To determine a given link availability, i.e. when a contact to a particular neighbor node exists, the scheduler reads a contact plan 
 * which is a JSON file that defines all the connections between all the nodes in the network.
 */


#ifndef SCHEDULER_H
#define SCHEDULER_H 1



#include <cstdint>
#include "zmq.hpp"
#include <memory>
#include <boost/bimap.hpp>
#include <boost/asio.hpp>
#include "HdtnConfig.h"
#include "HdtnDistributedConfig.h"
#include <boost/core/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include "router_lib_export.h"

#include "TimestampUtil.h"
#include "Logger.h"
#include <fstream>
#include "message.hpp"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>
#include <boost/make_unique.hpp>
#include <memory>
#include <fstream>
#include "TelemetryDefinitions.h"
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/bimap.hpp>
#include <cstdlib>
#include "Environment.h"
#include "JsonSerializable.h"
#include "codec/BundleViewV6.h"
#include "codec/BundleViewV7.h"

struct contactPlan_t {
    uint64_t contact;
    uint64_t source;
    uint64_t dest;
    uint64_t finalDest;//deprecated and not in operator <
    uint64_t start;
    uint64_t end;
    uint64_t rateBps;

    uint64_t outductArrayIndex; //not in operator <
    bool isLinkUp; 

    bool operator<(const contactPlan_t& o) const; //operator < so it can be used as a map key
};

struct OutductInfo_t {
    OutductInfo_t() : outductIndex(UINT64_MAX), nextHopNodeId(UINT64_MAX), linkIsUpTimeBased(false) {}
    OutductInfo_t(uint64_t paramOutductIndex, uint64_t paramNextHopNodeId, bool paramLinkIsUpTimeBased) :
        outductIndex(paramOutductIndex), nextHopNodeId(paramNextHopNodeId), linkIsUpTimeBased(paramLinkIsUpTimeBased) {}
    uint64_t outductIndex;
    uint64_t nextHopNodeId;
    bool linkIsUpTimeBased;
    
};

class Scheduler : private boost::noncopyable {
public:
    ROUTER_LIB_EXPORT Scheduler();
    ROUTER_LIB_EXPORT ~Scheduler();
    ROUTER_LIB_EXPORT void Stop();
    ROUTER_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const HdtnDistributedConfig& hdtnDistributedConfig,
        const boost::filesystem::path& contactPlanFilePath,
        bool usingUnixTimestamp,
        bool useMgr,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr = NULL);

    ROUTER_LIB_EXPORT static boost::filesystem::path GetFullyQualifiedFilename(const boost::filesystem::path& filename);
    ROUTER_LIB_EXPORT static uint64_t GetRateBpsFromPtree(const boost::property_tree::ptree::value_type& eventPtr);

private:
    bool ProcessContacts(const boost::property_tree::ptree & pt);
    bool ProcessContactsJsonText(char* jsonText);
    bool ProcessContactsJsonText(const std::string& jsonText);
    bool ProcessContactsFile(const boost::filesystem::path& jsonEventFilePath);

    void PopulateMapsFromAllOutductCapabilitiesTelemetry(AllOutductCapabilitiesTelemetry_t& aoct);
    void HandlePhysicalLinkStatusChange(const hdtn::LinkStatusHdr& linkStatusHdr);

    void NotifyEgressOfTimeBasedLinkChange(uint64_t outductArrayIndex, uint64_t rateBps, bool linkIsUpTimeBased);
    void SendLinkUp(uint64_t src, uint64_t dest, uint64_t outductArrayIndex,
        uint64_t time, uint64_t rateBps, uint64_t duration, bool isPhysical);
    void SendLinkDown(uint64_t src, uint64_t dest, uint64_t outductArrayIndex,
        uint64_t time, bool isPhysical);

    void EgressEventsHandler();
    bool SendBundle(const uint8_t* payloadData, const uint64_t payloadSizeBytes, const cbhe_eid_t& finalDestEid);
    void TelemEventsHandler();
    void ReadZmqAcksThreadFunc();
    void TryRestartContactPlanTimer();
    void OnContactPlan_TimerExpired(const boost::system::error_code& e);
    bool AddContact_NotThreadSafe(contactPlan_t& contact);

    /* From router */
    void HandleLinkDownEvent(const hdtn::IreleaseChangeHdr &releaseChangeHdr);
    void HandleLinkUpEvent(const hdtn::IreleaseChangeHdr &releaseChangeHdr);
    void HandleOutductCapabilitiesTelemetry(const hdtn::IreleaseChangeHdr &releaseChangeHdr, const AllOutductCapabilitiesTelemetry_t & aoct);
    void HandleBundleFromScheduler(const hdtn::IreleaseChangeHdr &releaseChangeHdr);

    void SendRouteUpdate(uint64_t nextHopNodeId, uint64_t finalDestNodeId);

    bool ComputeOptimalRoute(uint64_t sourceNode, uint64_t originalNextHopNodeId, uint64_t finalDestNodeId);
    bool ComputeOptimalRoutesForOutductIndex(uint64_t sourceNode, uint64_t outductIndex);

private:

    typedef std::pair<boost::posix_time::ptime, uint64_t> ptime_index_pair_t; //used in case of identical ptimes for starting events
    typedef boost::bimap<ptime_index_pair_t, contactPlan_t> ptime_to_contactplan_bimap_t;


    volatile bool m_running;
    HdtnConfig m_hdtnConfig;
    std::unique_ptr<boost::thread> m_threadZmqAckReaderPtr;

    std::unique_ptr<zmq::context_t> m_zmqCtxPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundEgressToConnectingSchedulerPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingRouterToBoundEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqXPubSock_boundSchedulerToConnectingSubsPtr;
    std::unique_ptr<zmq::socket_t> m_zmqRepSock_connectingTelemToFromBoundSchedulerPtr;
    boost::mutex m_mutexZmqPubSock;

    //no mutex needed (all run from ioService thread)
    std::map<uint64_t, OutductInfo_t> m_mapOutductArrayIndexToOutductInfo;
    std::map<uint64_t, uint64_t> m_mapNextHopNodeIdToOutductArrayIndex;

    boost::filesystem::path m_contactPlanFilePath;
    bool m_usingUnixTimestamp;

    ptime_to_contactplan_bimap_t m_ptimeToContactPlanBimap;
    boost::asio::io_service m_ioService;
    boost::asio::deadline_timer m_contactPlanTimer;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    bool m_contactPlanTimerIsRunning;
    boost::posix_time::ptime m_epoch;
    uint64_t m_subtractMeFromUnixTimeSecondsToConvertToSchedulerTimeSeconds;
    uint64_t m_numOutductCapabilityTelemetriesReceived;

    std::unique_ptr<zmq::message_t> m_zmqMessageOutductCapabilitiesTelemPtr;

    //for blocking until worker-thread startup
    volatile bool m_workerThreadStartupInProgress;
    boost::mutex m_workerThreadStartupMutex;
    boost::condition_variable m_workerThreadStartupConditionVariable;

    //send bundle stuff
    boost::mutex m_bundleCreationMutex;
    uint64_t m_lastMillisecondsSinceStartOfYear2000;
    uint64_t m_bundleSequence;

    /* From Router (non-duplicates) */
    
    bool m_usingMGR;
    bool m_computedInitialOptimalRoutes;
    uint64_t m_latestTime;

    typedef std::pair<uint64_t, std::list<uint64_t> > nexthop_finaldestlist_pair_t;
    std::map<uint64_t, nexthop_finaldestlist_pair_t> m_mapOutductArrayIndexToNextHopPlusFinalDestNodeIdList;

};

#endif // SCHEDULER_H
