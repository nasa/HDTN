/**
 * @file scheduler.cpp
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

#include "scheduler.h"
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

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::scheduler;

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

class Scheduler::Impl {
public:
    Impl();
    ~Impl();
    void Stop();
    bool Init(const HdtnConfig& hdtnConfig,
        const HdtnDistributedConfig& hdtnDistributedConfig,
        const boost::filesystem::path& contactPlanFilePath,
        bool usingUnixTimestamp,
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
    

private:
    typedef std::pair<boost::posix_time::ptime, uint64_t> ptime_index_pair_t; //used in case of identical ptimes for starting events
    typedef boost::bimap<ptime_index_pair_t, contactPlan_t> ptime_to_contactplan_bimap_t;


    volatile bool m_running;
    HdtnConfig m_hdtnConfig;
    std::unique_ptr<boost::thread> m_threadZmqAckReaderPtr;

    std::unique_ptr<zmq::context_t> m_zmqCtxPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundEgressToConnectingSchedulerPtr;
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
};



boost::filesystem::path Scheduler::GetFullyQualifiedFilename(const boost::filesystem::path& filename) {
    return (Environment::GetPathHdtnSourceRoot() / "module/scheduler/src/") / filename;
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


Scheduler::Impl::Impl() : 
    m_running(false), 
    m_usingUnixTimestamp(false),
    m_contactPlanTimerIsRunning(false),
    m_subtractMeFromUnixTimeSecondsToConvertToSchedulerTimeSeconds(0),
    m_numOutductCapabilityTelemetriesReceived(0),
    m_workerThreadStartupInProgress(false),
    m_lastMillisecondsSinceStartOfYear2000(0),
    m_bundleSequence(0),	
    m_contactPlanTimer(m_ioService) {}

Scheduler::Impl::~Impl() {
    Stop();
}

Scheduler::Scheduler() : m_pimpl(boost::make_unique<Scheduler::Impl>()) {}

Scheduler::~Scheduler() {
    Stop();
}

bool Scheduler::Init(const HdtnConfig& hdtnConfig,
    const HdtnDistributedConfig& hdtnDistributedConfig,
    const boost::filesystem::path& contactPlanFilePath,
    bool usingUnixTimestamp,
    zmq::context_t* hdtnOneProcessZmqInprocContextPtr)
{
    return m_pimpl->Init(hdtnConfig, hdtnDistributedConfig, contactPlanFilePath, usingUnixTimestamp, hdtnOneProcessZmqInprocContextPtr);
}

void Scheduler::Stop() {
    m_pimpl->Stop();
}
void Scheduler::Impl::Stop() {
    m_running = false; //thread stopping criteria

    if (m_threadZmqAckReaderPtr) {
        try {
            m_threadZmqAckReaderPtr->join(); 
            m_threadZmqAckReaderPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping Scheduler thread";
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

bool Scheduler::Impl::Init(const HdtnConfig& hdtnConfig,
    const HdtnDistributedConfig& hdtnDistributedConfig,
    const boost::filesystem::path& contactPlanFilePath,
    bool usingUnixTimestamp,
    zmq::context_t* hdtnOneProcessZmqInprocContextPtr)
{
    if (m_running) {
        LOG_ERROR(subprocess) << "Scheduler::Init called while Scheduler is already running";
        return false;
    }

    m_hdtnConfig = hdtnConfig;
    m_contactPlanFilePath = contactPlanFilePath;
    m_usingUnixTimestamp = usingUnixTimestamp;

    m_lastMillisecondsSinceStartOfYear2000 = 0;
    m_bundleSequence = 0;
    
    
    LOG_INFO(subprocess) << "initializing Scheduler..";

    m_ioService.reset();
    m_workPtr = boost::make_unique<boost::asio::io_service::work>(m_ioService);
    m_contactPlanTimerIsRunning = false;
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceScheduler");
 
    //socket for receiving events from Egress
    m_zmqCtxPtr = boost::make_unique<zmq::context_t>();

    if (hdtnOneProcessZmqInprocContextPtr) {
        m_zmqPullSock_boundEgressToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqRepSock_connectingTelemToFromBoundSchedulerPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        try {
            m_zmqPullSock_boundEgressToConnectingSchedulerPtr->connect(std::string("inproc://bound_egress_to_connecting_scheduler"));
            m_zmqRepSock_connectingTelemToFromBoundSchedulerPtr->bind(std::string("inproc://connecting_telem_to_from_bound_scheduler"));
        }
        catch (const zmq::error_t& ex) {
            LOG_ERROR(subprocess) << "error in Scheduler::Init: cannot connect inproc socket: " << ex.what();
            return false;
        }
    }
    else {
        m_zmqPullSock_boundEgressToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
        const std::string connect_boundEgressToConnectingSchedulerPath(
            std::string("tcp://") +
            hdtnDistributedConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqBoundEgressToConnectingSchedulerPortPath));
        try {
            m_zmqPullSock_boundEgressToConnectingSchedulerPtr->connect(connect_boundEgressToConnectingSchedulerPath);
            LOG_INFO(subprocess) << "Scheduler connected and listening to events from Egress " << connect_boundEgressToConnectingSchedulerPath;
        }
        catch (const zmq::error_t& ex) {
            LOG_ERROR(subprocess) << "error: scheduler cannot connect to egress socket: " << ex.what();
            return false;
        }
        m_zmqRepSock_connectingTelemToFromBoundSchedulerPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::rep);
        const std::string connect_connectingTelemToFromBoundSchedulerPath(
            std::string("tcp://*:") +
            boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingTelemToFromBoundSchedulerPortPath));
        try {
            m_zmqRepSock_connectingTelemToFromBoundSchedulerPtr->bind(connect_connectingTelemToFromBoundSchedulerPath);
            LOG_INFO(subprocess) << "Scheduler connected and listening to events from Telem " << connect_connectingTelemToFromBoundSchedulerPath;
        }
        catch (const zmq::error_t& ex) {
            LOG_ERROR(subprocess) << "error: scheduler cannot connect to telem socket: " << ex.what();
            return false;
        }
    }

    LOG_INFO(subprocess) << "Scheduler up and running";

    // Socket for sending events to Ingress, Storage, Router, and Egress
    m_zmqXPubSock_boundSchedulerToConnectingSubsPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::xpub);
    const std::string bind_boundSchedulerPubSubPath(
    std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));

    try {
        m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->bind(bind_boundSchedulerPubSubPath);
        LOG_INFO(subprocess) << "XPub socket bound successfully to " << bind_boundSchedulerPubSubPath;

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
            boost::bind(&Scheduler::Impl::ReadZmqAcksThreadFunc, this)); //create and start the worker thread

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

void Scheduler::Impl::SendLinkDown(uint64_t src, uint64_t dest, uint64_t outductArrayIndex,
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
    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        if(!m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->send(
            zmq::const_buffer(&stopMsg, sizeof(stopMsg)), zmq::send_flags::dontwait))
        {
            LOG_FATAL(subprocess) << "Cannot sent link down message to all subscribers";
        }
    }
    
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(subprocess) << " -- LINK DOWN Event sent for outductArrayIndex=" << outductArrayIndex
        << "  src(" << src << ") == = > dest(" << dest << ") at time " << timeLocal;
}

void Scheduler::Impl::NotifyEgressOfTimeBasedLinkChange(uint64_t outductArrayIndex, uint64_t rateBps, bool linkIsUpTimeBased) {
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
        if (!m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->send(
            zmq::const_buffer(&rateUpdateMsg, sizeof(rateUpdateMsg)), zmq::send_flags::dontwait))
        {
            LOG_FATAL(subprocess) << "Cannot send rate update message to egress";
        }
    }
}

void Scheduler::Impl::SendLinkUp(uint64_t src, uint64_t dest, uint64_t outductArrayIndex, uint64_t time, uint64_t rateBps, uint64_t duration, bool isPhysical) {
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
    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        if (!m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->send(
            zmq::const_buffer(&releaseMsg, sizeof(releaseMsg)), zmq::send_flags::dontwait))
        {
            LOG_FATAL(subprocess) << "Cannot sent link down message to all subscribers";
        }
    }

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(subprocess) << " -- LINK UP Event sent for outductArrayIndex=" << outductArrayIndex
        << "  src(" << src << ") == = > dest(" << dest << ") at time " << timeLocal;
}

void Scheduler::Impl::EgressEventsHandler() {
    //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
    hdtn::LinkStatusHdr linkStatusHdr;
    const zmq::recv_buffer_result_t res = m_zmqPullSock_boundEgressToConnectingSchedulerPtr->recv(zmq::mutable_buffer(&linkStatusHdr, sizeof(linkStatusHdr)), zmq::recv_flags::none);
    if (!res) {
        LOG_ERROR(subprocess) << "[EgressEventHandler] message not received";
        return;
    }
    else if (res->size != sizeof(linkStatusHdr)) {
        LOG_ERROR(subprocess) << "[EgressEventHandler] res->size != sizeof(linkStatusHdr)";
        return;
    }

    
    if (linkStatusHdr.base.type == HDTN_MSGTYPE_LINKSTATUS) {
        boost::asio::post(m_ioService, boost::bind(&Scheduler::Impl::HandlePhysicalLinkStatusChange, this, linkStatusHdr));
        
    }
    else if (linkStatusHdr.base.type == HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY) {
        AllOutductCapabilitiesTelemetry_t aoct;
        m_zmqMessageOutductCapabilitiesTelemPtr = boost::make_unique<zmq::message_t>();
        //message guaranteed to be there due to the zmq::send_flags::sndmore
        if (!m_zmqPullSock_boundEgressToConnectingSchedulerPtr->recv(*m_zmqMessageOutductCapabilitiesTelemPtr, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "error receiving AllOutductCapabilitiesTelemetry";
        }
        else if (!aoct.SetValuesFromJsonCharArray((char*)m_zmqMessageOutductCapabilitiesTelemPtr->data(), m_zmqMessageOutductCapabilitiesTelemPtr->size())) {
            LOG_ERROR(subprocess) << "error deserializing AllOutductCapabilitiesTelemetry";
        }
        else {
            LOG_INFO(subprocess) << "Scheduler received initial " << aoct.outductCapabilityTelemetryList.size() << " outduct telemetries from egress";

            boost::asio::post(m_ioService, boost::bind(&Scheduler::Impl::PopulateMapsFromAllOutductCapabilitiesTelemetry, this, std::move(aoct)));
    	    
            ++m_numOutductCapabilityTelemetriesReceived;
        }
    }
    else if (linkStatusHdr.base.type == HDTN_MSGTYPE_BUNDLES_TO_SCHEDULER) {
        zmq::message_t zmqMessageBundleToScheduler;
        //message guaranteed to be there due to the zmq::send_flags::sndmore
        if (!m_zmqPullSock_boundEgressToConnectingSchedulerPtr->recv(zmqMessageBundleToScheduler, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "error receiving zmqMessageBundleToScheduler";
        }
        else {
            uint8_t* bundleDataBegin = (uint8_t*)zmqMessageBundleToScheduler.data();
            const std::size_t bundleCurrentSize = zmqMessageBundleToScheduler.size();
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

                LOG_INFO(subprocess) << "scheduler received Bpv6 bundle with payload size " << payloadBlock.m_blockTypeSpecificDataLength;
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
                LOG_INFO(subprocess) << "scheduler received Bpv7 bundle with payload size " << payloadBlock.m_dataLength;
                //if (!ProcessPayload(payloadBlock.m_dataPtr, payloadBlock.m_dataLength)) {
            }
        }
        SendBundle((const uint8_t*)"scheduler bundle test payload!!!!", 33, cbhe_eid_t(2, 1));
    }
}

static void CustomCleanupStdVecUint8(void* data, void* hint) {
    delete static_cast<std::vector<uint8_t>*>(hint);
}

bool Scheduler::Impl::SendBundle(const uint8_t* payloadData, const uint64_t payloadSizeBytes, const cbhe_eid_t& finalDestEid) {
    // Next, send event to the rest of the modules
    hdtn::IreleaseChangeHdr releaseMsg;
    memset(&releaseMsg, 0, sizeof(releaseMsg));
    releaseMsg.SetSubscribeRouterAndIngressOnly(); //Router will ignore
    releaseMsg.base.type = HDTN_MSGTYPE_BUNDLES_FROM_SCHEDULER;


    BundleViewV7 bv;
    Bpv7CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;
    //primary.SetZero();
    primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
    primary.m_sourceNodeId.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_mySchedulerServiceId);
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
        bv.AppendMoveCanonicalBlock(payloadBlockPtr);
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
    std::vector<uint8_t>* vecUint8RawPointer = new std::vector<uint8_t>(std::move(bv.m_frontBuffer));
    zmq::message_t zmqSchedulerGeneratedBundle(vecUint8RawPointer->data(), vecUint8RawPointer->size(), CustomCleanupStdVecUint8, vecUint8RawPointer);
    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        if (!m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->send(
            zmq::const_buffer(&releaseMsg, sizeof(releaseMsg)), zmq::send_flags::sndmore | zmq::send_flags::dontwait))
        {
            LOG_FATAL(subprocess) << "Cannot sent HDTN_MSGTYPE_BUNDLES_FROM_SCHEDULER to ingress";
            return false;
        }
        if (!m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->send(std::move(zmqSchedulerGeneratedBundle), zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "Cannot sent zmqSchedulerGeneratedBundle to ingress";
            return false;
        }
    }
    return true;
}

void Scheduler::Impl::TelemEventsHandler() {
    uint8_t telemMsgByte;
    const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingTelemToFromBoundSchedulerPtr->recv(zmq::mutable_buffer(&telemMsgByte, sizeof(telemMsgByte)), zmq::recv_flags::dontwait);
    if (!res) {
        LOG_ERROR(subprocess) << "error in Scheduler::TelemEventsHandler: cannot read message";
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
            if (!m_zmqRepSock_connectingTelemToFromBoundSchedulerPtr->recv(apiMsg, zmq::recv_flags::none)) {
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
                    static_cast<bool (Scheduler::Impl::*) (const std::string&)>(&Scheduler::Impl::ProcessContactsJsonText),
                    this,
                    std::move(planJson)
                )
            );
            LOG_INFO(subprocess) << "received reload contact plan event with data " << uploadContactPlanApiCmd.m_contactPlanJson;
        } while (apiMsg.more());
        zmq::message_t msg;
        if (!m_zmqRepSock_connectingTelemToFromBoundSchedulerPtr->send(std::move(msg), zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "error replying to telem module";
        }
    }
}

void Scheduler::Impl::ReadZmqAcksThreadFunc() {
    ThreadNamer::SetThisThreadName("schedulerZmqReader");

    static constexpr unsigned int NUM_SOCKETS = 3;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_boundEgressToConnectingSchedulerPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqRepSock_connectingTelemToFromBoundSchedulerPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;
    bool schedulerFullyInitialized = false;
    bool egressSubscribed = false;
    bool ingressSubscribed = false;
    bool storageSubscribed = false;
    bool routerSubscribed = false;

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
                if (!m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->recv(zmqSubscriberDataReceived, zmq::recv_flags::none)) {
                    LOG_ERROR(subprocess) << "subscriber message not received";
                }
                else {
                    const uint8_t* const dataSubscriber = (const uint8_t*)zmqSubscriberDataReceived.data();
                    if ((zmqSubscriberDataReceived.size() == 2) && (dataSubscriber[1] == 'b')) {
                        egressSubscribed = (dataSubscriber[0] == 0x1);
                        LOG_INFO(subprocess) << "Egress " << ((egressSubscribed) ? "subscribed" : "desubscribed");
                    }
                    else if ((zmqSubscriberDataReceived.size() == 2) && (dataSubscriber[1] == 'a')) {
                        routerSubscribed = (dataSubscriber[0] == 0x1);
                        LOG_INFO(subprocess) << "Router " << ((routerSubscribed) ? "subscribed" : "desubscribed");
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

            if ((egressSubscribed) && (ingressSubscribed) && (storageSubscribed) && (routerSubscribed) && (m_zmqMessageOutductCapabilitiesTelemPtr)) {

                LOG_INFO(subprocess) << "Forwarding outduct capabilities telemetry to Router";
                hdtn::IreleaseChangeHdr releaseMsgHdr;
                releaseMsgHdr.SetSubscribeRouterOnly();
                releaseMsgHdr.base.type = HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY;

                {
                    boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
                    while (m_running && !m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->send(
                        zmq::const_buffer(&releaseMsgHdr, sizeof(releaseMsgHdr)), zmq::send_flags::sndmore | zmq::send_flags::dontwait))
                    {
                        LOG_INFO(subprocess) << "waiting for router to become available to send outduct capabilities header";
                        boost::this_thread::sleep(boost::posix_time::seconds(1));
                    }
                    if (!m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->send(std::move(*m_zmqMessageOutductCapabilitiesTelemPtr), zmq::send_flags::dontwait)) {
                        LOG_FATAL(subprocess) << "m_zmqXPubSock_boundSchedulerToConnectingSubsPtr could not send outduct capabilities";
                    }
                    m_zmqMessageOutductCapabilitiesTelemPtr.reset();
                }

                if (!schedulerFullyInitialized) {
                    //first time this outduct capabilities telemetry received, start remaining scheduler threads
                    schedulerFullyInitialized = true;
                    LOG_INFO(subprocess) << "Now running and fully initialized and connected to egress.. reading contact file " << m_contactPlanFilePath;
                    boost::asio::post(m_ioService, boost::bind(&Scheduler::Impl::ProcessContactsFile, this, m_contactPlanFilePath));
                }
                
            }
        }
    }
}

bool Scheduler::Impl::ProcessContactsJsonText(char * jsonText) {
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonCharArray(jsonText, strlen(jsonText), pt)) {
        return false;
    }
    return ProcessContacts(pt);
}
bool Scheduler::Impl::ProcessContactsJsonText(const std::string& jsonText) {
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonString(jsonText, pt)) {
        return false;
    }
    return ProcessContacts(pt);
}
bool Scheduler::Impl::ProcessContactsFile(const boost::filesystem::path& jsonEventFilePath) {
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonFilePath(jsonEventFilePath, pt)) {
        return false;
    }
    return ProcessContacts(pt);
}

uint64_t Scheduler::GetRateBpsFromPtree(const boost::property_tree::ptree::value_type& eventPtr)
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
bool Scheduler::Impl::ProcessContacts(const boost::property_tree::ptree& pt) {
    

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
        m_subtractMeFromUnixTimeSecondsToConvertToSchedulerTimeSeconds = 0;
    }
    else {
        LOG_INFO(subprocess) << "using now as epoch! ";
        m_epoch = boost::posix_time::microsec_clock::universal_time();

        const boost::posix_time::time_duration diff = (m_epoch - TimestampUtil::GetUnixEpoch());
        m_subtractMeFromUnixTimeSecondsToConvertToSchedulerTimeSeconds = static_cast<uint64_t>(diff.total_seconds());
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
        linkEvent.rateBps = Scheduler::GetRateBpsFromPtree(eventPt);
        if (linkEvent.dest == m_hdtnConfig.m_myNodeId) {
            LOG_WARNING(subprocess) << "Found a contact with destination (next hop node id) of " << m_hdtnConfig.m_myNodeId
                << " which is this HDTN's node id.. ignoring this unused contact from the contact plan.";
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
void Scheduler::Impl::TryRestartContactPlanTimer() {
    if (!m_contactPlanTimerIsRunning) {
        ptime_to_contactplan_bimap_t::left_iterator it = m_ptimeToContactPlanBimap.left.begin(); //get first event expiring
        if (it != m_ptimeToContactPlanBimap.left.end()) {
            const boost::posix_time::ptime& expiry = it->first.first;
            m_contactPlanTimer.expires_at(expiry);
            m_contactPlanTimer.async_wait(boost::bind(&Scheduler::Impl::OnContactPlan_TimerExpired, this, boost::asio::placeholders::error));
            m_contactPlanTimerIsRunning = true;
        }
        else {
            LOG_INFO(subprocess) << "End of ProcessEventFile";
        }
    }
}

void Scheduler::Impl::OnContactPlan_TimerExpired(const boost::system::error_code& e) {
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
                    const uint64_t now = TimestampUtil::GetSecondsSinceEpochUnix() - m_subtractMeFromUnixTimeSecondsToConvertToSchedulerTimeSeconds;
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

bool Scheduler::Impl::AddContact_NotThreadSafe(contactPlan_t& contact) {
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

void Scheduler::Impl::PopulateMapsFromAllOutductCapabilitiesTelemetry(AllOutductCapabilitiesTelemetry_t& aoct) {
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
            std::forward_as_tuple(oct.outductArrayIndex, oct.nextHopNodeId, false));
    }
}

void Scheduler::Impl::HandlePhysicalLinkStatusChange(const hdtn::LinkStatusHdr& linkStatusHdr) {
    const bool eventLinkIsUpPhysically = (linkStatusHdr.event == 1);
    const uint64_t outductArrayIndex = linkStatusHdr.uuid;
    const uint64_t timeSecondsSinceSchedulerEpoch = (linkStatusHdr.unixTimeSecondsSince1970 > m_subtractMeFromUnixTimeSecondsToConvertToSchedulerTimeSeconds) ?
        (linkStatusHdr.unixTimeSecondsSince1970 - m_subtractMeFromUnixTimeSecondsToConvertToSchedulerTimeSeconds) : 0;

    LOG_INFO(subprocess) << "Received physical link status " << ((eventLinkIsUpPhysically) ? "UP" : "DOWN")
        << " event from Egress for outductArrayIndex " << outductArrayIndex;

    std::map<uint64_t, OutductInfo_t>::const_iterator it = m_mapOutductArrayIndexToOutductInfo.find(outductArrayIndex);
    if (it == m_mapOutductArrayIndexToOutductInfo.cend()) {
        LOG_ERROR(subprocess) << "EgressEventsHandler got event for unknown outductArrayIndex " << outductArrayIndex << " which does not correspond to a next hop";
        return;
    }
    const OutductInfo_t& outductInfo = it->second;


    if (eventLinkIsUpPhysically) {
        if (outductInfo.linkIsUpTimeBased) {
            LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Up event at time  " << timeSecondsSinceSchedulerEpoch;
            SendLinkUp(m_hdtnConfig.m_myNodeId, outductInfo.nextHopNodeId, outductArrayIndex, timeSecondsSinceSchedulerEpoch, 0, 0, true);
        }
    }
    else {
        LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Down event at time  " << timeSecondsSinceSchedulerEpoch;

        SendLinkDown(m_hdtnConfig.m_myNodeId, outductInfo.nextHopNodeId, outductArrayIndex, timeSecondsSinceSchedulerEpoch, true);
    }
}
