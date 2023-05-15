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
#include "ThreadNamer.h"
#include "codec/BundleViewV6.h"
#include "codec/BundleViewV7.h"
#include "libcgr.h"

/** logger subprocess for the router */
static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::router;

/** A contact in the contact plan */
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

/** State for an outduct */
struct OutductInfo_t {
    OutductInfo_t()
        : outductIndex(UINT64_MAX), nextHopNodeId(UINT64_MAX), linkIsUpTimeBased(false), linkIsUpPhysical(false) {}
    OutductInfo_t(uint64_t paramOutductIndex, uint64_t paramNextHopNodeId, bool paramLinkIsUpTimeBased,
                  bool paramLinkIsUpPhysical, std::list<uint64_t> paramFinalDestNodeIdList)
        : outductIndex(paramOutductIndex), nextHopNodeId(paramNextHopNodeId), linkIsUpTimeBased(paramLinkIsUpTimeBased),
          linkIsUpPhysical(paramLinkIsUpPhysical), finalDestNodeIdList(paramFinalDestNodeIdList) {}

    uint64_t outductIndex;
    uint64_t nextHopNodeId;

    /** Does the contact plan consider this link to be up? */
    bool linkIsUpTimeBased;

    bool linkIsUpPhysical;

    std::list<uint64_t> finalDestNodeIdList;
};

/** Router private implementation class */
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
    void HandleBundle(const hdtn::IreleaseChangeHdr &releaseChangeHdr);

    void SendRouteUpdate(uint64_t nextHopNodeId, uint64_t finalDestNodeId);

    void FilterContactPlan(uint64_t sourceNode, std::vector<cgr::Contact> & contact_plan);
    bool ComputeOptimalRoute(uint64_t sourceNode, uint64_t originalNextHopNodeId, uint64_t finalDestNodeId);
    bool ComputeOptimalRoutesForOutductIndex(uint64_t sourceNode, uint64_t outductIndex);

private:

    typedef std::pair<boost::posix_time::ptime, uint64_t> ptime_index_pair_t; //used in case of identical ptimes for starting events
    typedef boost::bimap<ptime_index_pair_t, contactPlan_t> ptime_to_contactplan_bimap_t;


    volatile bool m_running;
    HdtnConfig m_hdtnConfig;
    std::unique_ptr<boost::thread> m_threadZmqAckReaderPtr;

    std::unique_ptr<zmq::context_t> m_zmqCtxPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundEgressToConnectingRouterPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingRouterToBoundEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqXPubSock_boundRouterToConnectingSubsPtr;
    std::unique_ptr<zmq::socket_t> m_zmqRepSock_connectingTelemToFromBoundRouterPtr;
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
    uint64_t m_subtractMeFromUnixTimeSecondsToConvertToRouterTimeSeconds;
    bool m_receivedInitialOutductTelemetry;

    std::unique_ptr<zmq::message_t> m_zmqMessageOutductCapabilitiesTelemPtr;

    //for blocking until worker-thread startup
    volatile bool m_workerThreadStartupInProgress;
    boost::mutex m_workerThreadStartupMutex;
    boost::condition_variable m_workerThreadStartupConditionVariable;

    //send bundle stuff
    boost::mutex m_bundleCreationMutex;
    uint64_t m_lastMillisecondsSinceStartOfYear2000;
    uint64_t m_bundleSequence;

    /* From Router */
    
    bool m_usingMGR;
    bool m_computedInitialOptimalRoutes;
    uint64_t m_latestTime;

};

boost::filesystem::path Router::GetFullyQualifiedFilename(const boost::filesystem::path& filename) {
    return (Environment::GetPathHdtnSourceRoot() / "module/router/contact_plans/") / filename;
}

bool contactPlan_t::operator<(const contactPlan_t& o) const {
    if (contact == o.contact) {
        if (source == o.source) {
            if (dest == o.dest) {
                if (isLinkUp == o.isLinkUp) {
                    return (start < o.start);
                }
                return (isLinkUp < o.isLinkUp);
            }
            return (dest < o.dest);
        }
        return (source < o.source);
    }
    return (contact < o.contact);
}


Router::Impl::Impl() : 
    m_running(false), 
    m_usingUnixTimestamp(false),
    m_contactPlanTimerIsRunning(false),
    m_subtractMeFromUnixTimeSecondsToConvertToRouterTimeSeconds(0),
    m_receivedInitialOutductTelemetry(false),
    m_workerThreadStartupInProgress(false),
    m_lastMillisecondsSinceStartOfYear2000(0),
    m_bundleSequence(0),	
    m_contactPlanTimer(m_ioService) {}

Router::Impl::~Impl() {
    Stop();
}

Router::Router() : m_pimpl(boost::make_unique<Router::Impl>()) {}

Router::~Router() {
    Stop();
}

bool Router::Init(const HdtnConfig& hdtnConfig,
    const HdtnDistributedConfig& hdtnDistributedConfig,
    const boost::filesystem::path& contactPlanFilePath,
    bool usingUnixTimestamp,
    bool useMgr,
    zmq::context_t* hdtnOneProcessZmqInprocContextPtr) {
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
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping Router thread";
        }
    }

    try {	
        m_contactPlanTimer.cancel();
    }
    catch (const boost::system::system_error&) {
        LOG_ERROR(subprocess) << "error cancelling contact plan timer ";
    }
	
    m_workPtr.reset();
    //This function does not block, but instead simply signals the io_service to stop
    //All invocations of its run() or run_one() member functions should return as soon as possible.
    //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
    //if (!m_ioService.stopped()) {
    //    m_ioService.stop(); //ioservice requires stopping before join because of the m_work object
    //}

    if (m_ioServiceThreadPtr) {
        try {
            m_ioServiceThreadPtr->join();
            m_ioServiceThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping io_service";
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
        LOG_ERROR(subprocess) << "Router::Impl::Init called while Router is already running";
        return false;
    }

    m_hdtnConfig = hdtnConfig;
    m_contactPlanFilePath = contactPlanFilePath;
    m_usingUnixTimestamp = usingUnixTimestamp;
    m_usingMGR = useMgr;
    m_computedInitialOptimalRoutes = false;

    m_lastMillisecondsSinceStartOfYear2000 = 0;
    m_bundleSequence = 0;
    
    
    LOG_INFO(subprocess) << "initializing Router..";

    m_ioService.reset();
    m_workPtr = boost::make_unique<boost::asio::io_service::work>(m_ioService);
    m_contactPlanTimerIsRunning = false;
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceRouter");
 
    //socket for receiving events from Egress
    m_zmqCtxPtr = boost::make_unique<zmq::context_t>();

    if (hdtnOneProcessZmqInprocContextPtr) {
        m_zmqPullSock_boundEgressToConnectingRouterPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPushSock_connectingRouterToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqRepSock_connectingTelemToFromBoundRouterPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        try {
            m_zmqPullSock_boundEgressToConnectingRouterPtr->connect(std::string("inproc://bound_egress_to_connecting_router"));
            m_zmqPushSock_connectingRouterToBoundEgressPtr->connect(std::string("inproc://connecting_router_to_bound_egress"));
            m_zmqRepSock_connectingTelemToFromBoundRouterPtr->bind(std::string("inproc://connecting_telem_to_from_bound_router"));
        }
        catch (const zmq::error_t& ex) {
            LOG_ERROR(subprocess) << "error in Router::Impl::Init: cannot connect inproc socket: " << ex.what();
            return false;
        }
    }
    else {
        m_zmqPullSock_boundEgressToConnectingRouterPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
        const std::string connect_boundEgressToConnectingRouterPath(
            std::string("tcp://") +
            hdtnDistributedConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqBoundEgressToConnectingRouterPortPath));
        try {
            m_zmqPullSock_boundEgressToConnectingRouterPtr->connect(connect_boundEgressToConnectingRouterPath);
            LOG_INFO(subprocess) << "Router connected and listening to events from Egress " << connect_boundEgressToConnectingRouterPath;
        }
        catch (const zmq::error_t& ex) {
            LOG_ERROR(subprocess) << "error: router cannot connect to egress socket: " << ex.what();
            return false;
        }

        m_zmqPushSock_connectingRouterToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
        const std::string connect_connectingRouterToBoundEgressPath(
            std::string("tcp://") +
            hdtnDistributedConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingRouterToBoundEgressPortPath));
        try {
            m_zmqPushSock_connectingRouterToBoundEgressPtr->connect(connect_connectingRouterToBoundEgressPath);
            LOG_INFO(subprocess) << "Router connected to Egress for sending events " << connect_connectingRouterToBoundEgressPath;
        }
        catch (const zmq::error_t& ex) {
            LOG_ERROR(subprocess) << "error: Router cannot connect to egress socket: " << connect_connectingRouterToBoundEgressPath;
            return false;
        }

        m_zmqRepSock_connectingTelemToFromBoundRouterPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::rep);
        const std::string connect_connectingTelemToFromBoundRouterPath(
            std::string("tcp://*:") +
            boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingTelemToFromBoundRouterPortPath));
        try {
            m_zmqRepSock_connectingTelemToFromBoundRouterPtr->bind(connect_connectingTelemToFromBoundRouterPath);
            LOG_INFO(subprocess) << "Router connected and listening to events from Telem " << connect_connectingTelemToFromBoundRouterPath;
        }
        catch (const zmq::error_t& ex) {
            LOG_ERROR(subprocess) << "error: router cannot connect to telem socket: " << ex.what();
            return false;
        }
    }

    //Caution: All options, with the exception of ZMQ_SUBSCRIBE, ZMQ_UNSUBSCRIBE and ZMQ_LINGER, only take effect for subsequent socket bind/connects.
    //The value of 0 specifies no linger period. Pending messages shall be discarded immediately when the socket is closed with zmq_close().
    m_zmqPushSock_connectingRouterToBoundEgressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr

    LOG_INFO(subprocess) << "Router up and running";

    // Socket for sending events to Ingress, Storage, Router, and Egress
    m_zmqXPubSock_boundRouterToConnectingSubsPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::xpub);
    const std::string bind_boundRouterPubSubPath(
    std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundRouterPubSubPortPath));

    try {
        m_zmqXPubSock_boundRouterToConnectingSubsPtr->bind(bind_boundRouterPubSubPath);
        LOG_INFO(subprocess) << "XPub socket bound successfully to " << bind_boundRouterPubSubPath;

    }
    catch (const zmq::error_t & ex) {
        LOG_ERROR(subprocess) << "XPub socket failed to bind: " << ex.what();
        return false;
    }

    { //launch worker thread
        m_running = true;

        boost::mutex::scoped_lock workerThreadStartupLock(m_workerThreadStartupMutex);
        m_workerThreadStartupInProgress = true;

        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Router::Impl::ReadZmqAcksThreadFunc, this)); //create and start the worker thread

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

    return true;
}

void Router::Impl::SendLinkDown(uint64_t src, uint64_t dest, uint64_t outductArrayIndex,
		             uint64_t time, bool isPhysical) {
    hdtn::IreleaseChangeHdr stopMsg;
    
    memset(&stopMsg, 0, sizeof(stopMsg));
    stopMsg.SetSubscribeAll();
    stopMsg.base.type = HDTN_MSGTYPE_ILINKDOWN;
    stopMsg.nextHopNodeId = dest;
    stopMsg.prevHopNodeId = src;
    stopMsg.outductArrayIndex = outductArrayIndex;
    stopMsg.time = time;
    stopMsg.isPhysical = (isPhysical ? 1 : 0);

    HandleLinkDownEvent(stopMsg);

    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        if(!m_zmqXPubSock_boundRouterToConnectingSubsPtr->send(
            zmq::const_buffer(&stopMsg, sizeof(stopMsg)), zmq::send_flags::dontwait))
        {
            LOG_FATAL(subprocess) << "Cannot sent link down message to all subscribers";
        }
    }
    
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(subprocess) << " -- LINK DOWN Event sent for outductArrayIndex=" << outductArrayIndex
        << "  src(" << src << ") == = > dest(" << dest << ") at time " << timeLocal;
}

void Router::Impl::NotifyEgressOfTimeBasedLinkChange(uint64_t outductArrayIndex, uint64_t rateBps, bool linkIsUpTimeBased) {
    // First, send rate update message to egress, so it has time to
    // update the rate before receiving date.
    // This message also serves for Egress to update telemetry of linkIsUpTimeBased for an outduct
    hdtn::IreleaseChangeHdr rateUpdateMsg;
    memset(&rateUpdateMsg, 0, sizeof(rateUpdateMsg));
    rateUpdateMsg.SetSubscribeEgressOnly();
    rateUpdateMsg.rateBps = rateBps;
    rateUpdateMsg.base.type = linkIsUpTimeBased ? HDTN_MSGTYPE_ILINKUP : HDTN_MSGTYPE_ILINKDOWN;
    rateUpdateMsg.outductArrayIndex = outductArrayIndex;
    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        if (!m_zmqXPubSock_boundRouterToConnectingSubsPtr->send(
            zmq::const_buffer(&rateUpdateMsg, sizeof(rateUpdateMsg)), zmq::send_flags::dontwait))
        {
            LOG_FATAL(subprocess) << "Cannot send rate update message to egress";
        }
    }
}

void Router::Impl::SendLinkUp(uint64_t src, uint64_t dest, uint64_t outductArrayIndex, uint64_t time, uint64_t rateBps, uint64_t duration, bool isPhysical) {
    // Send event to Ingress, Storage, and Router modules (not egress)
    hdtn::IreleaseChangeHdr releaseMsg;
    memset(&releaseMsg, 0, sizeof(releaseMsg));
    releaseMsg.SetSubscribeAll();
    releaseMsg.base.type = HDTN_MSGTYPE_ILINKUP;
    releaseMsg.nextHopNodeId = dest;
    releaseMsg.prevHopNodeId = src;
    releaseMsg.outductArrayIndex = outductArrayIndex;
    releaseMsg.time = time;
    releaseMsg.rateBps = rateBps;
    releaseMsg.duration = duration;
    releaseMsg.isPhysical = (isPhysical ? 1 : 0);

    HandleLinkUpEvent(releaseMsg);

    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        if (!m_zmqXPubSock_boundRouterToConnectingSubsPtr->send(
            zmq::const_buffer(&releaseMsg, sizeof(releaseMsg)), zmq::send_flags::dontwait))
        {
            LOG_FATAL(subprocess) << "Cannot sent link down message to all subscribers";
        }
    }

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(subprocess) << " -- LINK UP Event sent for outductArrayIndex=" << outductArrayIndex
        << "  src(" << src << ") == = > dest(" << dest << ") at time " << timeLocal;
}

void Router::Impl::EgressEventsHandler() {
    //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
    hdtn::LinkStatusHdr linkStatusHdr;
    const zmq::recv_buffer_result_t res = m_zmqPullSock_boundEgressToConnectingRouterPtr->recv(zmq::mutable_buffer(&linkStatusHdr, sizeof(linkStatusHdr)), zmq::recv_flags::none);
    if (!res) {
        LOG_ERROR(subprocess) << "[EgressEventHandler] message not received";
        return;
    }
    else if (res->size != sizeof(linkStatusHdr)) {
        LOG_ERROR(subprocess) << "[EgressEventHandler] res->size != sizeof(linkStatusHdr)";
        return;
    }

    
    if (linkStatusHdr.base.type == HDTN_MSGTYPE_LINKSTATUS) {
        boost::asio::post(m_ioService, boost::bind(&Router::Impl::HandlePhysicalLinkStatusChange, this, linkStatusHdr));
        
    }
    else if (linkStatusHdr.base.type == HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY) {
        AllOutductCapabilitiesTelemetry_t aoct;
        m_zmqMessageOutductCapabilitiesTelemPtr = boost::make_unique<zmq::message_t>();
        //message guaranteed to be there due to the zmq::send_flags::sndmore
        if (!m_zmqPullSock_boundEgressToConnectingRouterPtr->recv(*m_zmqMessageOutductCapabilitiesTelemPtr, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "error receiving AllOutductCapabilitiesTelemetry";
        }
        else if (!aoct.SetValuesFromJsonCharArray((char*)m_zmqMessageOutductCapabilitiesTelemPtr->data(), m_zmqMessageOutductCapabilitiesTelemPtr->size())) {
            LOG_ERROR(subprocess) << "error deserializing AllOutductCapabilitiesTelemetry";
        }
        else {
            LOG_INFO(subprocess) << "Router received initial " << aoct.outductCapabilityTelemetryList.size() << " outduct telemetries from egress";

            if(!m_receivedInitialOutductTelemetry) {
                boost::asio::post(m_ioService, boost::bind(&Router::Impl::PopulateMapsFromAllOutductCapabilitiesTelemetry, this, std::move(aoct)));
                m_receivedInitialOutductTelemetry = true;
            }
        }
    }
    else if (linkStatusHdr.base.type == HDTN_MSGTYPE_BUNDLES_TO_ROUTER) {
        zmq::message_t zmqMessageBundleToRouter;
        //message guaranteed to be there due to the zmq::send_flags::sndmore
        if (!m_zmqPullSock_boundEgressToConnectingRouterPtr->recv(zmqMessageBundleToRouter, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "error receiving zmqMessageBundleToRouter";
        }
        else {
            uint8_t* bundleDataBegin = (uint8_t*)zmqMessageBundleToRouter.data();
            const std::size_t bundleCurrentSize = zmqMessageBundleToRouter.size();
            const uint8_t firstByte = bundleDataBegin[0];
            const bool isBpVersion6 = (firstByte == 6);
            const bool isBpVersion7 = (firstByte == ((4U << 5) | 31U));  //CBOR major type 4, additional information 31 (Indefinite-Length Array)
            if (isBpVersion6) {
                BundleViewV6 bv;
                if (!bv.LoadBundle(bundleDataBegin, bundleCurrentSize)) {
                    LOG_ERROR(subprocess) << "malformed bundle";
                    return;
                }
                Bpv6CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;

                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                if (blocks.size() != 1) {
                    LOG_ERROR(subprocess) << "payload block not found";
                    return;
                }
                Bpv6CanonicalBlock& payloadBlock = *(blocks[0]->headerPtr);

                LOG_INFO(subprocess) << "router received Bpv6 bundle with payload size " << payloadBlock.m_blockTypeSpecificDataLength;
                //if (!ProcessPayload(payloadBlock.m_blockTypeSpecificDataPtr, payloadBlock.m_blockTypeSpecificDataLength)) {
                
            }
            else if (isBpVersion7) {
                BundleViewV7 bv;
                if (!bv.LoadBundle(bundleDataBegin, bundleCurrentSize)) { //invalid bundle
                    LOG_ERROR(subprocess) << "malformed bpv7 bundle";
                    return;
                }
                Bpv7CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;

                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
                bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);

                if (blocks.size() != 1) {
                    LOG_ERROR(subprocess) << "payload block not found";
                    return;
                }

                Bpv7CanonicalBlock& payloadBlock = *(blocks[0]->headerPtr);
                LOG_INFO(subprocess) << "router received Bpv7 bundle with payload size " << payloadBlock.m_dataLength;
                //if (!ProcessPayload(payloadBlock.m_dataPtr, payloadBlock.m_dataLength)) {
            }
        }
        SendBundle((const uint8_t*)"router bundle test payload!!!!", 33, cbhe_eid_t(2, 1));
    }
}

static void CustomCleanupPaddedVecUint8(void* data, void* hint) {
    delete static_cast<padded_vector_uint8_t*>(hint);
}

bool Router::Impl::SendBundle(const uint8_t* payloadData, const uint64_t payloadSizeBytes, const cbhe_eid_t& finalDestEid) {
    // Next, send event to the rest of the modules
    hdtn::IreleaseChangeHdr releaseMsg;
    memset(&releaseMsg, 0, sizeof(releaseMsg));
    releaseMsg.SetSubscribeRouterAndIngressOnly(); //Router will ignore
    releaseMsg.base.type = HDTN_MSGTYPE_BUNDLES_FROM_ROUTER;


    BundleViewV7 bv;
    Bpv7CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;
    //primary.SetZero();
    primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
    primary.m_sourceNodeId.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_myRouterServiceId);
    primary.m_destinationEid = finalDestEid;
    primary.m_reportToEid.Set(0, 0);
    primary.m_creationTimestamp.SetTimeFromNow();
    {
        boost::mutex::scoped_lock lock(m_bundleCreationMutex);
        if (primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 == m_lastMillisecondsSinceStartOfYear2000) {
            ++m_bundleSequence;
        }
        else {
            m_bundleSequence = 0;
        }
        m_lastMillisecondsSinceStartOfYear2000 = primary.m_creationTimestamp.millisecondsSinceStartOfYear2000;
        primary.m_creationTimestamp.sequenceNumber = m_bundleSequence;
    }
    primary.m_lifetimeMilliseconds = 1000000;
    primary.m_crcType = BPV7_CRC_TYPE::CRC32C;
    bv.m_primaryBlockView.SetManuallyModified();

    

    //append payload block (must be last block)
    {
        std::unique_ptr<Bpv7CanonicalBlock> payloadBlockPtr = boost::make_unique<Bpv7CanonicalBlock>();
        Bpv7CanonicalBlock& payloadBlock = *payloadBlockPtr;
        //payloadBlock.SetZero();

        payloadBlock.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
        payloadBlock.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;
        payloadBlock.m_blockNumber = 1; //must be 1
        payloadBlock.m_crcType = BPV7_CRC_TYPE::CRC32C;
        payloadBlock.m_dataLength = payloadSizeBytes;
        payloadBlock.m_dataPtr = NULL; //NULL will preallocate (won't copy or compute crc, user must do that manually below)
        bv.AppendMoveCanonicalBlock(std::move(payloadBlockPtr));
    }

    //render bundle to the front buffer
    if (!bv.Render(payloadSizeBytes + 1000)) {
        LOG_ERROR(subprocess) << "error rendering bpv7 bundle";
        return false;
    }

    BundleViewV7::Bpv7CanonicalBlockView& payloadBlockView = bv.m_listCanonicalBlockView.back(); //payload block must be the last block

    //manually copy data to preallocated space and compute crc
    memcpy(payloadBlockView.headerPtr->m_dataPtr, payloadData, payloadSizeBytes);
    
    payloadBlockView.headerPtr->RecomputeCrcAfterDataModification((uint8_t*)payloadBlockView.actualSerializedBlockPtr.data(), payloadBlockView.actualSerializedBlockPtr.size()); //recompute crc

    //move the bundle out of bundleView
    padded_vector_uint8_t* vecUint8RawPointer = new padded_vector_uint8_t(std::move(bv.m_frontBuffer));
    zmq::message_t zmqRouterGeneratedBundle(vecUint8RawPointer->data(), vecUint8RawPointer->size(), CustomCleanupPaddedVecUint8, vecUint8RawPointer);
    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);

        HandleBundle(releaseMsg);

        if (!m_zmqXPubSock_boundRouterToConnectingSubsPtr->send(
            zmq::const_buffer(&releaseMsg, sizeof(releaseMsg)), zmq::send_flags::sndmore | zmq::send_flags::dontwait))
        {
            LOG_FATAL(subprocess) << "Cannot sent HDTN_MSGTYPE_BUNDLES_FROM_ROUTER to ingress";
            return false;
        }
        if (!m_zmqXPubSock_boundRouterToConnectingSubsPtr->send(std::move(zmqRouterGeneratedBundle), zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "Cannot sent zmqRouterGeneratedBundle to ingress";
            return false;
        }
    }
    return true;
}

void Router::Impl::TelemEventsHandler() {
    uint8_t telemMsgByte;
    const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingTelemToFromBoundRouterPtr->recv(zmq::mutable_buffer(&telemMsgByte, sizeof(telemMsgByte)), zmq::recv_flags::dontwait);
    if (!res) {
        LOG_ERROR(subprocess) << "error in Router::Impl::TelemEventsHandler: cannot read message";
    }
    else if ((res->truncated()) || (res->size != sizeof(telemMsgByte))) {
        LOG_ERROR(subprocess) << "TelemEventsHandler message mismatch: untruncated = " << res->untruncated_size
            << " truncated = " << res->size << " expected = " << sizeof(hdtn::ToEgressHdr);
    }
    else {
        const bool hasApiCalls = telemMsgByte > TELEM_REQ_MSG;
        if (!hasApiCalls) {
            return;
        }
        zmq::message_t apiMsg;
        do {
            if (!m_zmqRepSock_connectingTelemToFromBoundRouterPtr->recv(apiMsg, zmq::recv_flags::none)) {
                LOG_ERROR(subprocess) << "[TelemEventsHandler] message not received";
                return;
            }
            std::string apiCall = ApiCommand_t::GetApiCallFromJson(apiMsg.to_string());
            LOG_INFO(subprocess) << "Got an api call " << apiMsg.to_string();
            if (apiCall != "upload_contact_plan") {
                return;
            }
            UploadContactPlanApiCommand_t uploadContactPlanApiCmd;
            uploadContactPlanApiCmd.SetValuesFromJson(apiMsg.to_string());
            std::string planJson = uploadContactPlanApiCmd.m_contactPlanJson;
            boost::asio::post(
                m_ioService,
                boost::bind(
                    static_cast<bool (Router::Impl::*) (const std::string&)>(&Router::Impl::ProcessContactsJsonText),
                    this,
                    std::move(planJson)
                )
            );
            LOG_INFO(subprocess) << "received reload contact plan event with data " << uploadContactPlanApiCmd.m_contactPlanJson;
        } while (apiMsg.more());
        zmq::message_t msg;
        if (!m_zmqRepSock_connectingTelemToFromBoundRouterPtr->send(std::move(msg), zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "error replying to telem module";
        }
    }
}

void Router::Impl::ReadZmqAcksThreadFunc() {
    ThreadNamer::SetThisThreadName("routerZmqReader");

    static constexpr unsigned int NUM_SOCKETS = 3;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_boundEgressToConnectingRouterPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqRepSock_connectingTelemToFromBoundRouterPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqXPubSock_boundRouterToConnectingSubsPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;
    bool routerFullyInitialized = false;
    bool egressSubscribed = false;
    bool ingressSubscribed = false;
    bool storageSubscribed = false;

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;

    //notify Init function that worker thread startup is complete
    m_workerThreadStartupMutex.lock();
    m_workerThreadStartupInProgress = false;
    m_workerThreadStartupMutex.unlock();
    m_workerThreadStartupConditionVariable.notify_one();

    while (m_running) { //keep thread alive if running
        int rc = 0;
        try {
            rc = zmq::poll(items, NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
        }
        catch (zmq::error_t & e) {
            LOG_ERROR(subprocess) << "caught zmq::error_t in Ingress::ReadZmqAcksThreadFunc: " << e.what();
            continue;
        }
        if (rc > 0) {
            if (items[0].revents & ZMQ_POLLIN) { //events from Egress
                EgressEventsHandler();
            }
            if (items[1].revents & ZMQ_POLLIN) { //events from Telemetry
                TelemEventsHandler();
            }
            if (items[2].revents & ZMQ_POLLIN) { //subscriber events from xsub sockets for release messages
                zmq::message_t zmqSubscriberDataReceived;
                //Subscription message is a byte 1 (for subscriptions) or byte 0 (for unsubscriptions) followed by the subscription body.
                //All release messages shall be prefixed by "aaaaaaaa" before the common header
                //Router unique subscription shall be "a" (gets all messages that start with "a") (e.g "aaa", "ab", etc.)
                //Ingress unique subscription shall be "aa"
                //Storage unique subscription shall be "aaa"
                boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
                if (!m_zmqXPubSock_boundRouterToConnectingSubsPtr->recv(zmqSubscriberDataReceived, zmq::recv_flags::none)) {
                    LOG_ERROR(subprocess) << "subscriber message not received";
                }
                else {
                    const uint8_t* const dataSubscriber = (const uint8_t*)zmqSubscriberDataReceived.data();
                    if ((zmqSubscriberDataReceived.size() == 2) && (dataSubscriber[1] == 'b')) {
                        egressSubscribed = (dataSubscriber[0] == 0x1);
                        LOG_INFO(subprocess) << "Egress " << ((egressSubscribed) ? "subscribed" : "desubscribed");
                    }
                    else if ((zmqSubscriberDataReceived.size() == 3) && (dataSubscriber[1] == 'a') && (dataSubscriber[2] == 'a')) {
                        ingressSubscribed = (dataSubscriber[0] == 0x1);
                        LOG_INFO(subprocess) << "Ingress " << ((ingressSubscribed) ? "subscribed" : "desubscribed");
                    }
                    else if ((zmqSubscriberDataReceived.size() == 4) && (dataSubscriber[1] == 'a') && (dataSubscriber[2] == 'a') && ((dataSubscriber[3] == 'a'))) {
                        storageSubscribed = (dataSubscriber[0] == 0x1);
                        LOG_INFO(subprocess) << "Storage " << ((storageSubscribed) ? "subscribed" : "desubscribed");
                    }
                    else if ((zmqSubscriberDataReceived.size() == 9) &&
                        (dataSubscriber[1] == 'a') && 
                        (dataSubscriber[2] == 'a') &&
                        (dataSubscriber[3] == 'a') &&
                        (dataSubscriber[4] == 'a') &&
                        (dataSubscriber[5] == 'a') &&
                        (dataSubscriber[6] == 'a') &&
                        (dataSubscriber[7] == 'a') &&
                        (dataSubscriber[8] == 'a')) {
                        bool uisSubscribed = (dataSubscriber[0] == 0x1);
                        LOG_INFO(subprocess) << "UIS " << ((uisSubscribed) ? "subscribed" : "desubscribed");
                    }
                    else {
                        LOG_ERROR(subprocess) << "invalid subscriber message received: length=" << zmqSubscriberDataReceived.size();
                    }
                }
            }

            if ((egressSubscribed) && (ingressSubscribed) && (storageSubscribed) && (m_zmqMessageOutductCapabilitiesTelemPtr)) {

                LOG_INFO(subprocess) << "Forwarding outduct capabilities telemetry to Router";
                hdtn::IreleaseChangeHdr releaseMsgHdr;
                releaseMsgHdr.SetSubscribeRouterOnly();
                releaseMsgHdr.base.type = HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY;

                //boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
                AllOutductCapabilitiesTelemetry_t aoct;
                if (!aoct.SetValuesFromJsonCharArray((char*)m_zmqMessageOutductCapabilitiesTelemPtr->data(), m_zmqMessageOutductCapabilitiesTelemPtr->size())) {
                    HandleOutductCapabilitiesTelemetry(releaseMsgHdr, aoct);
                } else {
                    LOG_ERROR(subprocess) << "error deserializing AllOutductCapabilitiesTelemetry";
                }

                m_zmqMessageOutductCapabilitiesTelemPtr.reset();

                if (!routerFullyInitialized) {
                    //first time this outduct capabilities telemetry received, start remaining router threads
                    routerFullyInitialized = true;
                    LOG_INFO(subprocess) << "Now running and fully initialized and connected to egress.. reading contact file " << m_contactPlanFilePath;
                    boost::asio::post(m_ioService, boost::bind(&Router::Impl::ProcessContactsFile, this, m_contactPlanFilePath));
                }
                
            }
        }
    }
}

bool Router::Impl::ProcessContactsJsonText(char * jsonText) {
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonCharArray(jsonText, strlen(jsonText), pt)) {
        return false;
    }
    return ProcessContacts(pt);
}
bool Router::Impl::ProcessContactsJsonText(const std::string& jsonText) {
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonString(jsonText, pt)) {
        return false;
    }
    return ProcessContacts(pt);
}
bool Router::Impl::ProcessContactsFile(const boost::filesystem::path& jsonEventFilePath) {
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonFilePath(jsonEventFilePath, pt)) {
        return false;
    }
    return ProcessContacts(pt);
}

uint64_t Router::GetRateBpsFromPtree(const boost::property_tree::ptree::value_type& eventPtr)
{
    // First, attempt to get "rateBitsPerSec"
    try {
        return eventPtr.second.get<uint64_t>("rateBitsPerSec");
    }
    catch (const boost::property_tree::ptree_error&) {
        LOG_WARNING(subprocess) << "rateBps not defined in contact plan";
    }

    // If that fails, attempt to get deprecated "rate", which is in mbps
    try {
        const uint64_t rateMbps = eventPtr.second.get<uint64_t>("rate");
        LOG_WARNING(subprocess) << "[DEPRECATED] rate field in contact plan. Use 'rateBitsPerSec'";
        return rateMbps * 1000000;
    }
    catch(const boost::property_tree::ptree_error&) {
        LOG_WARNING(subprocess) << "failed to find rateBps or rate in contact plan. Using default.";
    }
    return 0;
}

//must only be run from ioService thread because maps unprotected (no mutex)
bool Router::Impl::ProcessContacts(const boost::property_tree::ptree& pt) {
    

    m_contactPlanTimer.cancel(); //cancel any running contacts in the timer

    //cancel any existing contacts (make them all link down) (ignore link up) in preparation for new contact plan
    for (std::map<uint64_t, OutductInfo_t>::iterator it = m_mapOutductArrayIndexToOutductInfo.begin(); it != m_mapOutductArrayIndexToOutductInfo.end(); ++it) {
        OutductInfo_t& outductInfo = it->second;
        if (outductInfo.linkIsUpTimeBased) {
            LOG_INFO(subprocess) << "Reloading contact plan: changing time based link up to link down for source "
                << m_hdtnConfig.m_myNodeId << " destination " << outductInfo.nextHopNodeId << " outductIndex " << outductInfo.outductIndex;
            SendLinkDown(m_hdtnConfig.m_myNodeId, outductInfo.nextHopNodeId, outductInfo.outductIndex, 0, false);
            outductInfo.linkIsUpTimeBased = false;
        }
    }


    m_ptimeToContactPlanBimap.clear(); //clear the map

    if (m_usingUnixTimestamp) {
        LOG_INFO(subprocess) << "***Using unix timestamp! ";
        m_epoch = TimestampUtil::GetUnixEpoch();
        m_subtractMeFromUnixTimeSecondsToConvertToRouterTimeSeconds = 0;
    }
    else {
        LOG_INFO(subprocess) << "using now as epoch! ";
        m_epoch = boost::posix_time::microsec_clock::universal_time();

        const boost::posix_time::time_duration diff = (m_epoch - TimestampUtil::GetUnixEpoch());
        m_subtractMeFromUnixTimeSecondsToConvertToRouterTimeSeconds = static_cast<uint64_t>(diff.total_seconds());
    }

    //for non-throw versions of get_child which return a reference to the second parameter
    static const boost::property_tree::ptree EMPTY_PTREE;
    
    const boost::property_tree::ptree& contactsPt = pt.get_child("contacts", EMPTY_PTREE);
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, contactsPt) {
        contactPlan_t linkEvent;
        linkEvent.contact = eventPt.second.get<int>("contact", 0);
        linkEvent.source = eventPt.second.get<int>("source", 0);
        linkEvent.dest = eventPt.second.get<int>("dest", 0);
        linkEvent.finalDest = eventPt.second.get<int>("finalDestination", 0);
        linkEvent.start = eventPt.second.get<int>("startTime", 0);
        linkEvent.end = eventPt.second.get<int>("endTime", 0);
        linkEvent.rateBps = Router::GetRateBpsFromPtree(eventPt);
        if (linkEvent.dest == m_hdtnConfig.m_myNodeId) {
            LOG_WARNING(subprocess) << "Found a contact with destination (next hop node id) of " << m_hdtnConfig.m_myNodeId
                << " which is this HDTN's node id.. ignoring this unused contact from the contact plan.";
            continue;
        }
        if(linkEvent.source != m_hdtnConfig.m_myNodeId) {
            LOG_WARNING(subprocess) << "Found a contact with source of " << linkEvent.source
                << " which is not this HDTN's node id... ignoring this contact from the contact plan";
            continue;
        }
        std::map<uint64_t, uint64_t>::const_iterator it = m_mapNextHopNodeIdToOutductArrayIndex.find(linkEvent.dest);
        if (it != m_mapNextHopNodeIdToOutductArrayIndex.cend()) {
            linkEvent.outductArrayIndex = it->second;
            if (!AddContact_NotThreadSafe(linkEvent)) {
                LOG_WARNING(subprocess) << "failed to add a contact";
            }
        }
        else {
            LOG_WARNING(subprocess) << "Found a contact with destination (next hop node id) of " << linkEvent.dest
                << " which isn't in the HDTN outductVector.. ignoring this unused contact from the contact plan.";
        }
    }

    LOG_INFO(subprocess) << "Epoch Time:  " << m_epoch;

    m_contactPlanTimerIsRunning = false;
    TryRestartContactPlanTimer(); //wait for next event (do this after all sockets initialized)

    return true;
}


//restarts the timer if it is not running
void Router::Impl::TryRestartContactPlanTimer() {
    if (!m_contactPlanTimerIsRunning) {
        ptime_to_contactplan_bimap_t::left_iterator it = m_ptimeToContactPlanBimap.left.begin(); //get first event expiring
        if (it != m_ptimeToContactPlanBimap.left.end()) {
            const boost::posix_time::ptime& expiry = it->first.first;
            m_contactPlanTimer.expires_at(expiry);
            m_contactPlanTimer.async_wait(boost::bind(&Router::Impl::OnContactPlan_TimerExpired, this, boost::asio::placeholders::error));
            m_contactPlanTimerIsRunning = true;
        }
        else {
            LOG_INFO(subprocess) << "End of ProcessEventFile";
        }
    }
}

void Router::Impl::OnContactPlan_TimerExpired(const boost::system::error_code& e) {
    m_contactPlanTimerIsRunning = false;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        ptime_to_contactplan_bimap_t::left_iterator it = m_ptimeToContactPlanBimap.left.begin(); //get event that started the timer
        if (it != m_ptimeToContactPlanBimap.left.end()) {
            const contactPlan_t& contactPlan = it->second;
            LOG_INFO(subprocess) << ((contactPlan.isLinkUp) ? "LINK UP" : "LINK DOWN") << " (time based) for source "
                << contactPlan.source << " destination " << contactPlan.dest;

            std::map<uint64_t, OutductInfo_t>::iterator outductInfoIt = m_mapOutductArrayIndexToOutductInfo.find(contactPlan.outductArrayIndex);
            if (outductInfoIt == m_mapOutductArrayIndexToOutductInfo.cend()) {
                LOG_ERROR(subprocess) << "OnContactPlan_TimerExpired got event for unknown outductArrayIndex "
                    << contactPlan.outductArrayIndex;
            }
            else {
                //update linkIsUpTimeBased in the outductInfo
                OutductInfo_t& outductInfo = outductInfoIt->second;
                outductInfo.linkIsUpTimeBased = contactPlan.isLinkUp;
                NotifyEgressOfTimeBasedLinkChange(contactPlan.outductArrayIndex, contactPlan.rateBps, outductInfo.linkIsUpTimeBased);
                if (outductInfo.linkIsUpTimeBased) {
                    const uint64_t now = TimestampUtil::GetSecondsSinceEpochUnix() - m_subtractMeFromUnixTimeSecondsToConvertToRouterTimeSeconds;
                    const uint64_t durationStart = std::max(contactPlan.start, now);
                    const uint64_t duration = contactPlan.end < durationStart ?  0 : (contactPlan.end - durationStart);
                    SendLinkUp(contactPlan.source, contactPlan.dest, contactPlan.outductArrayIndex, contactPlan.start, contactPlan.rateBps, duration, false);
                }
                else {
                    SendLinkDown(contactPlan.source, contactPlan.dest, contactPlan.outductArrayIndex, contactPlan.end + 1, false);
                }
            }

            m_ptimeToContactPlanBimap.left.erase(it);
            TryRestartContactPlanTimer(); //wait for next event
        }
    }
}

bool Router::Impl::AddContact_NotThreadSafe(contactPlan_t& contact) {
    {
        ptime_index_pair_t pipStart(m_epoch + boost::posix_time::seconds(contact.start), 0);
        while (m_ptimeToContactPlanBimap.left.count(pipStart)) {
            pipStart.second += 1; //in case of events that occur at the same time
        }
        contact.isLinkUp = true; //true => add link up event
        if (!m_ptimeToContactPlanBimap.insert(ptime_to_contactplan_bimap_t::value_type(pipStart, contact)).second) {
            return false;
        }
    }
    {
        ptime_index_pair_t pipEnd(m_epoch + boost::posix_time::seconds(contact.end), 0);
        while (m_ptimeToContactPlanBimap.left.count(pipEnd)) {
            pipEnd.second += 1; //in case of events that occur at the same time
        }
        contact.isLinkUp = false; //false => add link down event
        if (!m_ptimeToContactPlanBimap.insert(ptime_to_contactplan_bimap_t::value_type(pipEnd, contact)).second) {
            return false;
        }
    }
    return true;
}

void Router::Impl::PopulateMapsFromAllOutductCapabilitiesTelemetry(AllOutductCapabilitiesTelemetry_t& aoct) {
    m_mapOutductArrayIndexToOutductInfo.clear();
    m_mapNextHopNodeIdToOutductArrayIndex.clear();

    for (std::list<OutductCapabilityTelemetry_t>::const_iterator itAoct = aoct.outductCapabilityTelemetryList.cbegin();
        itAoct != aoct.outductCapabilityTelemetryList.cend(); ++itAoct)
    {
        const OutductCapabilityTelemetry_t& oct = *itAoct;
        m_mapNextHopNodeIdToOutductArrayIndex[oct.nextHopNodeId] = oct.outductArrayIndex;
        m_mapOutductArrayIndexToOutductInfo.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(oct.outductArrayIndex),
            std::forward_as_tuple(oct.outductArrayIndex, oct.nextHopNodeId, false, false, std::list<uint64_t>()));
    }
}

void Router::Impl::HandlePhysicalLinkStatusChange(const hdtn::LinkStatusHdr& linkStatusHdr) {
    const bool eventLinkIsUpPhysically = (linkStatusHdr.event == 1);
    const uint64_t outductArrayIndex = linkStatusHdr.uuid;
    const uint64_t timeSecondsSinceRouterEpoch = (linkStatusHdr.unixTimeSecondsSince1970 > m_subtractMeFromUnixTimeSecondsToConvertToRouterTimeSeconds) ?
        (linkStatusHdr.unixTimeSecondsSince1970 - m_subtractMeFromUnixTimeSecondsToConvertToRouterTimeSeconds) : 0;

    LOG_INFO(subprocess) << "Received physical link status " << ((eventLinkIsUpPhysically) ? "UP" : "DOWN")
        << " event from Egress for outductArrayIndex " << outductArrayIndex;

    std::map<uint64_t, OutductInfo_t>::iterator it = m_mapOutductArrayIndexToOutductInfo.find(outductArrayIndex);
    if (it == m_mapOutductArrayIndexToOutductInfo.end()) {
        LOG_ERROR(subprocess) << "EgressEventsHandler got event for unknown outductArrayIndex " << outductArrayIndex << " which does not correspond to a next hop";
        return;
    }
    OutductInfo_t& outductInfo = it->second;

    outductInfo.linkIsUpPhysical = eventLinkIsUpPhysically;

    if (eventLinkIsUpPhysically) {
        if (outductInfo.linkIsUpTimeBased) {
            LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Up event at time  " << timeSecondsSinceRouterEpoch;
            SendLinkUp(m_hdtnConfig.m_myNodeId, outductInfo.nextHopNodeId, outductArrayIndex, timeSecondsSinceRouterEpoch, 0, 0, true);
        }
    }
    else {
        LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Down event at time  " << timeSecondsSinceRouterEpoch;

        SendLinkDown(m_hdtnConfig.m_myNodeId, outductInfo.nextHopNodeId, outductArrayIndex, timeSecondsSinceRouterEpoch, true);
    }
}

void Router::Impl::SendRouteUpdate(uint64_t nextHopNodeId, uint64_t finalDestNodeId) {
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();

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

/* From Router */

void Router::Impl::HandleLinkDownEvent(const hdtn::IreleaseChangeHdr &releaseChangeHdr) {
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

void Router::Impl::HandleLinkUpEvent(const hdtn::IreleaseChangeHdr &releaseChangeHdr) {
    m_latestTime = releaseChangeHdr.time;
    LOG_DEBUG(subprocess) << "Contact up ";
    LOG_DEBUG(subprocess) << "Updated time to " << m_latestTime;
}

void Router::Impl::HandleOutductCapabilitiesTelemetry(const hdtn::IreleaseChangeHdr &releaseChangeHdr, const AllOutductCapabilitiesTelemetry_t & aoct) {
    m_latestTime = releaseChangeHdr.time;
    LOG_INFO(subprocess) << "Received and successfully decoded " << ((m_computedInitialOptimalRoutes) ? "UPDATED" : "NEW")
        << " AllOutductCapabilitiesTelemetry_t message from Router containing "
        << aoct.outductCapabilityTelemetryList.size() << " outducts.";

    for (std::list<OutductCapabilityTelemetry_t>::const_iterator itAoct = aoct.outductCapabilityTelemetryList.cbegin();
        itAoct != aoct.outductCapabilityTelemetryList.cend(); ++itAoct)
    {
        const OutductCapabilityTelemetry_t& oct = *itAoct;
        std::map<uint64_t, OutductInfo_t>::iterator it =  m_mapOutductArrayIndexToOutductInfo.find(oct.outductArrayIndex);
        if(it == m_mapOutductArrayIndexToOutductInfo.end()) {
            LOG_ERROR(subprocess) << "could not find outduct info for index while updating routes";
            continue;
        }
        OutductInfo_t & info = it->second;
        info.finalDestNodeIdList.clear();

        for (std::list<uint64_t>::const_iterator it = oct.finalDestinationNodeIdList.cbegin();
            it != oct.finalDestinationNodeIdList.cend(); ++it)
        {
            const uint64_t nodeId = *it;
            info.finalDestNodeIdList.emplace_back(nodeId);
            LOG_INFO(subprocess) << "Compute Optimal Route for finalDestination node " << nodeId;
        }
        std::set<uint64_t> usedNodeIds;
        for (std::list<cbhe_eid_t>::const_iterator it = oct.finalDestinationEidList.cbegin();
            it != oct.finalDestinationEidList.cend(); ++it)
        {
            const uint64_t nodeId = it->nodeId;
            if (usedNodeIds.emplace(nodeId).second) { //was inserted
                info.finalDestNodeIdList.emplace_back(nodeId);
                LOG_INFO(subprocess) << "Compute Optimal Route for finalDestination node " << nodeId;
            }
        }
        if (!m_computedInitialOptimalRoutes) {
            ComputeOptimalRoutesForOutductIndex(m_hdtnConfig.m_myNodeId, oct.outductArrayIndex);
        }
    }
    m_computedInitialOptimalRoutes = true;

}

/** Placeholder */
void Router::Impl::HandleBundle(const hdtn::IreleaseChangeHdr &releaseChangeHdr) {
    m_latestTime = releaseChangeHdr.time;
}

/** Filter "failed" contacts
 *
 * Remove from the contact plan contacts which are active (i.e.
 * happening now), have this node as the source, and for which
 * the link to the neighbor node is down
 *
 * @param contactPlan - the contact plan to modify in-place
 */
void Router::Impl::FilterContactPlan(uint64_t sourceNode, std::vector<cgr::Contact> & contactPlan) {
	
    int i = 0;
    while(i < contactPlan.size()) {
        const cgr::Contact & contact = contactPlan[i];

        // Don't remove if not "active"
        if(!(contact.start < m_latestTime && m_latestTime < contact.end)) {
            ++i;
            continue;
        }

        // Don't remove if we're not the source
        if(contact.frm != sourceNode) {
            ++i;
            continue;
        }

        // Don't remove if not associated with one of our outducts
        if(m_mapNextHopNodeIdToOutductArrayIndex.count(contact.to) == 0) {
            ++i;
            continue;
        }
        uint64_t outduct_index = m_mapNextHopNodeIdToOutductArrayIndex[contact.to];
        const OutductInfo_t & info = m_mapOutductArrayIndexToOutductInfo[outduct_index];

        // Don't remove if link is up
        if(info.linkIsUpPhysical) {
            ++i;
            continue;
        }

        // Otherwise: active contact from our node with link DOWN,
        // remove contact from contact plan to re-route around down node
        contactPlan.erase(contactPlan.begin() + i);
    }
}

/** Recompute routes for destination eids associated with an outduct
 *
 * @param sourceNode - the source node ID for the routes
 * @param outductIndex - the outduct index
 *
 * @returns true on success, false on failure
 *
 * Sends updated routes to egress
 *
 */
bool Router::Impl::ComputeOptimalRoutesForOutductIndex(uint64_t sourceNode, uint64_t outductIndex) {

    std::map<uint64_t, OutductInfo_t>::const_iterator it = m_mapOutductArrayIndexToOutductInfo.find(outductIndex);
    if (it == m_mapOutductArrayIndexToOutductInfo.cend()) {
        LOG_ERROR(subprocess) << "ComputeOptimalRoutesForOutductIndex cannot find outductIndex " << outductIndex;
        return false;
    }

    const OutductInfo_t &info = it->second;
    bool noErrors = true;
    for (std::list<uint64_t>::const_iterator it = info.finalDestNodeIdList.cbegin();
        it != info.finalDestNodeIdList.cend(); ++it)
    {
        const uint64_t finalDestNodeId = *it;
        LOG_DEBUG(subprocess) << "FinalDest nodeId found is:  " << finalDestNodeId;
        if (!ComputeOptimalRoute(sourceNode, info.nextHopNodeId, finalDestNodeId)) {
            noErrors = false;
        }
    }
    return noErrors;
}

/** Compute optimal route to destination
 *
 * @param sourceNode the starting node for the route
 * @param originalNextHopNodeId the current route
 * @param the final destination node ID
 *
 * Computes the optimal route for a node. If the route is different,
 * sends a route update message to egress
 *
 * @returns false on error, true on success
 */
bool Router::Impl::ComputeOptimalRoute(uint64_t sourceNode, uint64_t originalNextHopNodeId, uint64_t finalDestNodeId) {

    cgr::Route bestRoute;

    LOG_DEBUG(subprocess) << "Reading contact plan and computing next hop";
    std::vector<cgr::Contact> contactPlan = cgr::cp_load(m_contactPlanFilePath);

    // The contact plan here is a copy of the actual contact plan,
    // filtering only affects this instance of this function call
    FilterContactPlan(sourceNode, contactPlan);

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
