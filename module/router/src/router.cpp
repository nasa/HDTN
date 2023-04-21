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

class Router::Impl {
public:
    Impl();
    ~Impl();
    void Stop();
    bool Init(const HdtnConfig& hdtnConfig,
        const HdtnDistributedConfig& hdtnDistributedConfig,
        const boost::filesystem::path& contactPlanFilePath,
        bool usingUnixTimestamp,
        bool useMgr,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr);

private:
    bool ComputeOptimalRoute(uint64_t sourceNode, uint64_t originalNextHopNodeId, uint64_t finalDestNodeId);
    bool ComputeOptimalRoutesForOutductIndex(uint64_t sourceNode, uint64_t outductIndex);
    void ReadZmqThreadFunc();
    void SchedulerEventsHandler();

    HdtnConfig m_hdtnConfig;
    volatile bool m_running;
    bool m_usingUnixTimestamp;
    bool m_usingMGR;
    bool m_computedInitialOptimalRoutes;
    uint64_t m_latestTime;
    boost::filesystem::path m_contactPlanFilePath;

    std::unique_ptr<zmq::context_t> m_zmqContextPtr;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundSchedulerToConnectingRouterPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingRouterToBoundEgressPtr;

    typedef std::pair<uint64_t, std::list<uint64_t> > nexthop_finaldestlist_pair_t;
    std::map<uint64_t, nexthop_finaldestlist_pair_t> m_mapOutductArrayIndexToNextHopPlusFinalDestNodeIdList;
    std::unique_ptr<boost::thread> m_threadZmqAckReaderPtr;

    
    //for blocking until worker-thread startup
    volatile bool m_workerThreadStartupInProgress;
    boost::mutex m_workerThreadStartupMutex;
    boost::condition_variable m_workerThreadStartupConditionVariable;
};

boost::filesystem::path Router::GetFullyQualifiedFilename(const boost::filesystem::path& filename) {
    return (Environment::GetPathHdtnSourceRoot() / "module/router/contact_plans/") / filename;
}

Router::Impl::Impl() : 
    m_running(false), 
    m_computedInitialOptimalRoutes(false), 
    m_latestTime(0), 
    m_workerThreadStartupInProgress(false), 
    m_usingUnixTimestamp(false), 
    m_usingMGR(false) {}

Router::Router() : m_pimpl(boost::make_unique<Router::Impl>()) {}
    
Router::Impl::~Impl() {
    Stop();
}

Router::~Router() {
    Stop();
}

bool Router::Init(const HdtnConfig& hdtnConfig,
    const HdtnDistributedConfig& hdtnDistributedConfig,
    const boost::filesystem::path& contactPlanFilePath,
    bool usingUnixTimestamp,
    bool useMgr,
    zmq::context_t* hdtnOneProcessZmqInprocContextPtr)
{
    return m_pimpl->Init(hdtnConfig, hdtnDistributedConfig, contactPlanFilePath, usingUnixTimestamp, useMgr, hdtnOneProcessZmqInprocContextPtr);
}

void Router::Stop() {
    m_pimpl->Stop();
}
void Router::Impl::Stop() {
    m_running = false; //thread stopping criteria
    
    if (m_threadZmqAckReaderPtr) { 
        try {
            m_threadZmqAckReaderPtr->join();
            m_threadZmqAckReaderPtr.reset(); //delete it
        } catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping Router thread";
        }
    }
}

bool Router::Impl::Init(const HdtnConfig& hdtnConfig,
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


    m_zmqSubSock_boundSchedulerToConnectingRouterPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::sub);
    const std::string connect_boundSchedulerPubSubPath(
        std::string("tcp://") +
        ((hdtnOneProcessZmqInprocContextPtr == NULL) ? hdtnDistributedConfig.m_zmqSchedulerAddress : std::string("localhost")) +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));
    try {
        m_zmqSubSock_boundSchedulerToConnectingRouterPtr->connect(connect_boundSchedulerPubSubPath);
        m_zmqSubSock_boundSchedulerToConnectingRouterPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        LOG_INFO(subprocess) << "Connected to scheduler at " << connect_boundSchedulerPubSubPath << " , subscribing...";
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(subprocess) << "Cannot connect to scheduler socket at " << connect_boundSchedulerPubSubPath << " : " << ex.what();
        return false;
    }


    try {
        static const int timeout = 250;  // milliseconds
        m_zmqSubSock_boundSchedulerToConnectingRouterPtr->set(zmq::sockopt::rcvtimeo, timeout);
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(subprocess) << "error: cannot set timeout on receive sockets: " << ex.what();
        return false;
    }

    { //launch worker thread
        m_running = true;

        boost::mutex::scoped_lock workerThreadStartupLock(m_workerThreadStartupMutex);
        m_workerThreadStartupInProgress = true;

        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Router::Impl::ReadZmqThreadFunc, this)); //create and start the worker thread

        while (m_workerThreadStartupInProgress) { //lock mutex (above) before checking condition
            //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
            if (!m_workerThreadStartupConditionVariable.timed_wait(workerThreadStartupLock, boost::posix_time::seconds(3))) {
                LOG_ERROR(subprocess) << "timed out waiting (for 3 seconds) for worker thread to start up";
                break;
            }
        }
        if (m_workerThreadStartupInProgress) {
            LOG_ERROR(subprocess) << "error: worker thread took too long to start up.. exiting";
            return false;
        }
        else {
            LOG_INFO(subprocess) << "worker thread started";
        }
    }

    LOG_INFO(subprocess) << "Router up and running";
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(subprocess) << "Router currentTime  " << timeLocal;
    return true;
}

void Router::Impl::SchedulerEventsHandler() {

    const uint64_t srcNode = m_hdtnConfig.m_myNodeId;

    hdtn::IreleaseChangeHdr releaseChangeHdr;

    const zmq::recv_buffer_result_t res =
        m_zmqSubSock_boundSchedulerToConnectingRouterPtr->recv(zmq::mutable_buffer(&releaseChangeHdr,
            sizeof(releaseChangeHdr)), zmq::recv_flags::none);
    if (!res) {
        LOG_ERROR(subprocess) << "unable to receive message";
    }
    else if (res->size != sizeof(releaseChangeHdr)) {
        LOG_ERROR(subprocess) << "res->size != sizeof(releaseChangeHdr)";
        return;
    }

    m_latestTime = releaseChangeHdr.time;

    if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKDOWN) {
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
    else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKUP) {
        LOG_DEBUG(subprocess) << "Contact up ";
        LOG_DEBUG(subprocess) << "Updated time to " << m_latestTime;
    }
    else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY) {
        AllOutductCapabilitiesTelemetry_t aoct;
        zmq::message_t zmqMessageOutductTelem;
        //message guaranteed to be there due to the zmq::send_flags::sndmore
        if (!m_zmqSubSock_boundSchedulerToConnectingRouterPtr->recv(zmqMessageOutductTelem, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "error receiving AllOutductCapabilitiesTelemetry";
        }
        else if (!aoct.SetValuesFromJsonCharArray((char*)zmqMessageOutductTelem.data(), zmqMessageOutductTelem.size())) {
            LOG_ERROR(subprocess) << "error deserializing AllOutductCapabilitiesTelemetry of size " << zmqMessageOutductTelem.size();
        }
        else {
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
                    ComputeOptimalRoutesForOutductIndex(srcNode, oct.outductArrayIndex);
                }
            }
            m_computedInitialOptimalRoutes = true;
            
        }
    }
    else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_BUNDLES_FROM_SCHEDULER) {
        //ignore but must discard multi-part message
        zmq::message_t zmqMessageDiscard;
        if (!m_zmqSubSock_boundSchedulerToConnectingRouterPtr->recv(zmqMessageDiscard, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "Error discarding Bundle From Scheduler Message";
        }
    }
    else {
        LOG_ERROR(subprocess) << "[Router] unknown message type " << releaseChangeHdr.base.type;
    }
}

void Router::Impl::ReadZmqThreadFunc() {

    ThreadNamer::SetThisThreadName("routerZmqReader");

    static constexpr unsigned int NUM_SOCKETS = 1;
    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqSubSock_boundSchedulerToConnectingRouterPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;

    m_computedInitialOptimalRoutes = false;

    //notify Init function that worker thread startup is complete
    m_workerThreadStartupMutex.lock();
    m_workerThreadStartupInProgress = false;
    m_workerThreadStartupMutex.unlock();
    m_workerThreadStartupConditionVariable.notify_one();

    try {
        //Sends one-byte 0x1 message to scheduler XPub socket plus strlen of subscription
        //All release messages shall be prefixed by "aaaaaaaa" before the common header
        //Router unique subscription shall be "a" (gets all messages that start with "a") (e.g "aaa", "ab", etc.)
        //Ingress unique subscription shall be "aa"
        //Storage unique subscription shall be "aaa"
        m_zmqSubSock_boundSchedulerToConnectingRouterPtr->set(zmq::sockopt::subscribe, "a");
        LOG_INFO(subprocess) << "Subscribed to all events from scheduler";
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(subprocess) << "Cannot subscribe to all events from scheduler: " << ex.what();
        //return false;
    }

    while (m_running) {
        int rc = 0;
        try {
            rc = zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
        }
        catch (zmq::error_t& e) {
            LOG_ERROR(subprocess) << "zmq::poll threw zmq::error_t in hdtn::Router::Run: " << e.what();
            continue;
        }
        if (rc > 0) {
            if (items[0].revents & ZMQ_POLLIN) { //events from scheduler
                SchedulerEventsHandler();
            }
        }
    }
}

bool Router::Impl::ComputeOptimalRoutesForOutductIndex(uint64_t sourceNode, uint64_t outductIndex) {
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

bool Router::Impl::ComputeOptimalRoute(uint64_t sourceNode, uint64_t originalNextHopNodeId, uint64_t finalDestNodeId) {

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
