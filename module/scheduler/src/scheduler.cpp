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


static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::scheduler;

struct contactPlan_t {
    uint64_t contact;
    uint64_t source;
    uint64_t dest;
    uint64_t finalDest;//deprecated and not in operator <
    uint64_t start;
    uint64_t end;
    uint64_t rate;

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
        const boost::filesystem::path& contactPlanFilePath,
        bool usingUnixTimestamp,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr);

private:
    bool ProcessContactsPtPtr(std::shared_ptr<boost::property_tree::ptree>& contactsPtPtr);
    bool ProcessContacts(const boost::property_tree::ptree & pt);
    bool ProcessContactsJsonText(char* jsonText);
    bool ProcessContactsJsonText(const std::string& jsonText);
    bool ProcessContactsFile(const boost::filesystem::path& jsonEventFilePath);

    void PopulateMapsFromAllOutductCapabilitiesTelemetry(AllOutductCapabilitiesTelemetry_t& aoct);
    void HandlePhysicalLinkStatusChange(const hdtn::LinkStatusHdr& linkStatusHdr);

    void SendLinkUp(uint64_t src, uint64_t dest, uint64_t outductArrayIndex,
            uint64_t time);
    void SendLinkDown(uint64_t src, uint64_t dest, uint64_t outductArrayIndex,
            uint64_t time);

    void EgressEventsHandler();
    void UisEventsHandler();
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
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundUisToConnectingSchedulerPtr;
    std::unique_ptr<zmq::socket_t> m_zmqXPubSock_boundSchedulerToConnectingSubsPtr;
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


Scheduler::Impl::Impl() : m_running(false), m_contactPlanTimer(m_ioService) {}

Scheduler::Impl::~Impl() {
    Stop();
}

Scheduler::Scheduler() : m_pimpl(boost::make_unique<Scheduler::Impl>()) {}

Scheduler::~Scheduler() {
    Stop();
}




bool Scheduler::Init(const HdtnConfig& hdtnConfig,
    const boost::filesystem::path& contactPlanFilePath,
    bool usingUnixTimestamp,
    zmq::context_t* hdtnOneProcessZmqInprocContextPtr)
{
    return m_pimpl->Init(hdtnConfig, contactPlanFilePath, usingUnixTimestamp, hdtnOneProcessZmqInprocContextPtr);
}

void Scheduler::Stop() {
    m_pimpl->Stop();
}
void Scheduler::Impl::Stop() {
    m_running = false; //thread stopping criteria
    if (m_threadZmqAckReaderPtr) {
        m_threadZmqAckReaderPtr->join();
        m_threadZmqAckReaderPtr.reset(); //delete it
    }

    m_contactPlanTimer.cancel();

    m_workPtr.reset();
    //This function does not block, but instead simply signals the io_service to stop
    //All invocations of its run() or run_one() member functions should return as soon as possible.
    //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
    //if (!m_ioService.stopped()) {
    //    m_ioService.stop(); //ioservice requires stopping before join because of the m_work object
    //}

    if (m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }
}

bool Scheduler::Impl::Init(const HdtnConfig& hdtnConfig,
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
    
    
    LOG_INFO(subprocess) << "initializing Scheduler..";

    m_ioService.reset();
    m_workPtr = boost::make_unique<boost::asio::io_service::work>(m_ioService);
    m_contactPlanTimerIsRunning = false;
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

 
    //socket for receiving events from Egress
    m_zmqCtxPtr = boost::make_unique<zmq::context_t>();

    if (hdtnOneProcessZmqInprocContextPtr) {
        m_zmqPullSock_boundEgressToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        try {
            m_zmqPullSock_boundEgressToConnectingSchedulerPtr->connect(std::string("inproc://bound_egress_to_connecting_scheduler"));
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
            m_hdtnConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressToBoundSchedulerPortPath));
        try {
            m_zmqPullSock_boundEgressToConnectingSchedulerPtr->connect(connect_boundEgressToConnectingSchedulerPath);
            LOG_INFO(subprocess) << "Scheduler connected and listening to events from Egress " << connect_boundEgressToConnectingSchedulerPath;
        }
        catch (const zmq::error_t& ex) {
            LOG_ERROR(subprocess) << "error: scheduler cannot connect to egress socket: " << ex.what();
            return false;
        }
    }

    //socket for receiving events from UIS
    m_zmqSubSock_boundUisToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::sub);
    const std::string connect_boundUisPubSubPath(
        std::string("tcp://") +
        std::string("localhost") +
        std::string(":") +
        boost::lexical_cast<std::string>(29001)); //TODO
    try {
        m_zmqSubSock_boundUisToConnectingSchedulerPtr->connect(connect_boundUisPubSubPath);
        m_zmqSubSock_boundUisToConnectingSchedulerPtr->set(zmq::sockopt::subscribe, "");
        LOG_INFO(subprocess) << "Scheduler connected and listening to events from UIS " << connect_boundUisPubSubPath;
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(subprocess) << "error: scheduler cannot connect to UIS socket: " << ex.what();
        return false;
    }

    LOG_INFO(subprocess) << "Scheduler up and running";

    // Socket for sending events to Ingress, Storage and Router
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
		             uint64_t time) {

    hdtn::IreleaseChangeHdr stopMsg;
    
    memset(&stopMsg, 0, sizeof(stopMsg));
    stopMsg.SetSubscribeAll();
    stopMsg.base.type = HDTN_MSGTYPE_ILINKDOWN;
    stopMsg.nextHopNodeId = dest;
    stopMsg.prevHopNodeId = src;
    stopMsg.outductArrayIndex = outductArrayIndex;
    stopMsg.time = time;
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

void Scheduler::Impl::SendLinkUp(uint64_t src, uint64_t dest, uint64_t outductArrayIndex, uint64_t time) {
    
    hdtn::IreleaseChangeHdr releaseMsg;
    
    memset(&releaseMsg, 0, sizeof(releaseMsg));
    releaseMsg.SetSubscribeAll();
    releaseMsg.base.type = HDTN_MSGTYPE_ILINKUP;
    releaseMsg.nextHopNodeId = dest;
    releaseMsg.prevHopNodeId = src;
    releaseMsg.outductArrayIndex = outductArrayIndex;
    releaseMsg.time = time;
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
        uint64_t numBytesTakenToDecode;
        m_zmqMessageOutductCapabilitiesTelemPtr = boost::make_unique<zmq::message_t>();
        //message guaranteed to be there due to the zmq::send_flags::sndmore
        if (!m_zmqPullSock_boundEgressToConnectingSchedulerPtr->recv(*m_zmqMessageOutductCapabilitiesTelemPtr, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "error receiving AllOutductCapabilitiesTelemetry";
        }
        else if (!aoct.DeserializeFromLittleEndian((uint8_t*)m_zmqMessageOutductCapabilitiesTelemPtr->data(), numBytesTakenToDecode, m_zmqMessageOutductCapabilitiesTelemPtr->size())) {
            LOG_ERROR(subprocess) << "error deserializing AllOutductCapabilitiesTelemetry";
        }
        else {
            LOG_INFO(subprocess) << "Scheduler received initial " << aoct.outductCapabilityTelemetryList.size() << " outduct telemetries from egress";
            LOG_INFO(subprocess) << "Telemetry message content: " << aoct ;

            boost::asio::post(m_ioService, boost::bind(&Scheduler::Impl::PopulateMapsFromAllOutductCapabilitiesTelemetry, this, std::move(aoct)));
    	    
            ++m_numOutductCapabilityTelemetriesReceived;
        }
    }
}

void Scheduler::Impl::UisEventsHandler() {
    hdtn::ContactPlanReloadHdr hdr;
    const zmq::recv_buffer_result_t res = m_zmqSubSock_boundUisToConnectingSchedulerPtr->recv(zmq::mutable_buffer(&hdr, sizeof(hdr)), zmq::recv_flags::none);
    if (!res) {
        LOG_ERROR(subprocess) << "error in Scheduler::UisEventsHandler: cannot read hdr";
    }
    else if ((res->truncated()) || (res->size != sizeof(hdr))) {
        LOG_ERROR(subprocess) << "UisEventsHandler hdr message mismatch: untruncated = " << res->untruncated_size
            << " truncated = " << res->size << " expected = " << sizeof(hdtn::ToEgressHdr);
    }
    else if (hdr.base.type == CPM_NEW_CONTACT_PLAN) {
        zmq::message_t message;
        if (!m_zmqSubSock_boundUisToConnectingSchedulerPtr->recv(message, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "[UisEventsHandler] message not received";
            return;
        }
        boost::property_tree::ptree pt;
        if (!JsonSerializable::GetPropertyTreeFromJsonCharArray((char*)message.data(), message.size(), pt)) {
            LOG_ERROR(subprocess) << "[UisEventsHandler] JSON message invalid";
        }
        std::shared_ptr<boost::property_tree::ptree> ptPtr = std::make_shared<boost::property_tree::ptree>(pt);
        boost::asio::post(m_ioService, boost::bind(&Scheduler::Impl::ProcessContactsPtPtr, this, std::move(ptPtr)));
        LOG_INFO(subprocess) << "received Reload contact Plan event with data " << (char*)message.data();
    }
    else {
        LOG_ERROR(subprocess) << "error in Scheduler::UisEventsHandler: unknown hdr " << hdr.base.type;
    }

    
}

void Scheduler::Impl::ReadZmqAcksThreadFunc() {

    static constexpr unsigned int NUM_SOCKETS = 3;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_boundEgressToConnectingSchedulerPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundUisToConnectingSchedulerPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;
    bool schedulerFullyInitialized = false;
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
            if (items[1].revents & ZMQ_POLLIN) { //events from UIS
                UisEventsHandler();
            }
            if (items[2].revents & ZMQ_POLLIN) { //subscriber events from xsub sockets for release messages
                zmq::message_t zmqSubscriberDataReceived;
                //Subscription message is a byte 1 (for subscriptions) or byte 0 (for unsubscriptions) followed by the subscription body.
                //All release messages shall be prefixed by "aaaaaaaa" before the common header
                //Router unique subscription shall be "a" (gets all messages that start with "a") (e.g "aaa", "ab", etc.)
                //Ingress unique subscription shall be "aa"
                //Storage unique subscription shall be "aaa"
                if (!m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->recv(zmqSubscriberDataReceived, zmq::recv_flags::none)) {
                    LOG_ERROR(subprocess) << "subscriber message not received";
                }
                else {
                    const uint8_t* const dataSubscriber = (const uint8_t*)zmqSubscriberDataReceived.data();
                    if ((zmqSubscriberDataReceived.size() == 2) && (dataSubscriber[1] == 'a')) {
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
                    else {
                        LOG_ERROR(subprocess) << "invalid subscriber message received: length=" << zmqSubscriberDataReceived.size();
                    }
                }
            }

            if ((ingressSubscribed) && (storageSubscribed) && (routerSubscribed) && (m_zmqMessageOutductCapabilitiesTelemPtr)) {

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

bool Scheduler::Impl::ProcessContactsPtPtr(std::shared_ptr<boost::property_tree::ptree>& contactsPtPtr) {
    return ProcessContacts(*contactsPtPtr);
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

//must only be run from ioService thread because maps unprotected (no mutex)
bool Scheduler::Impl::ProcessContacts(const boost::property_tree::ptree& pt) {
    

    m_contactPlanTimer.cancel(); //cancel any running contacts in the timer

    //cancel any existing contacts (make them all link down) (ignore link up) in preparation for new contact plan
    for (std::map<uint64_t, OutductInfo_t>::iterator it = m_mapOutductArrayIndexToOutductInfo.begin(); it != m_mapOutductArrayIndexToOutductInfo.end(); ++it) {
        OutductInfo_t& outductInfo = it->second;
        if (outductInfo.linkIsUpTimeBased) {
            LOG_INFO(subprocess) << "Reloading contact plan: changing time based link up to link down for source "
                << m_hdtnConfig.m_myNodeId << " destination " << outductInfo.nextHopNodeId << " outductIndex " << outductInfo.outductIndex;
            SendLinkDown(m_hdtnConfig.m_myNodeId, outductInfo.nextHopNodeId, outductInfo.outductIndex, 0);
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
        linkEvent.rate = eventPt.second.get<int>("rate", 0);
        std::map<uint64_t, uint64_t>::const_iterator it = m_mapNextHopNodeIdToOutductArrayIndex.find(linkEvent.dest);
        if (it != m_mapNextHopNodeIdToOutductArrayIndex.cend()) {
            linkEvent.outductArrayIndex = it->second;
            if (!AddContact_NotThreadSafe(linkEvent)) {
                LOG_WARNING(subprocess) << "failed to add a contact";
            }
        }
        else {
            LOG_ERROR(subprocess) << "ProcessContacts could not find outduct array index for next hop " << linkEvent.dest;
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

            if (contactPlan.isLinkUp) {
                SendLinkUp(contactPlan.source, contactPlan.dest, contactPlan.outductArrayIndex, contactPlan.start);
            }
            else {
                SendLinkDown(contactPlan.source, contactPlan.dest, contactPlan.outductArrayIndex, contactPlan.end + 1);
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
        LOG_ERROR(subprocess) << "EgressEventsHandler got event for unknown outductArrayIndex " << outductArrayIndex << " which does not correspont to a next hop";
        return;
    }
    const OutductInfo_t& outductInfo = it->second;


    if (eventLinkIsUpPhysically) {
        if (outductInfo.linkIsUpTimeBased) {
            LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Up event at time  " << timeSecondsSinceSchedulerEpoch;
            SendLinkUp(m_hdtnConfig.m_myNodeId, outductInfo.nextHopNodeId, outductArrayIndex, timeSecondsSinceSchedulerEpoch);
        }
    }
    else {
        LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Down event at time  " << timeSecondsSinceSchedulerEpoch;

        SendLinkDown(m_hdtnConfig.m_myNodeId, outductInfo.nextHopNodeId, outductArrayIndex, timeSecondsSinceSchedulerEpoch);
    }
}
