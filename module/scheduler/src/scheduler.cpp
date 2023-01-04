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
#include "SignalHandler.h"
#include "TimestampUtil.h"
#include "Logger.h"
#include <fstream>
#include "message.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>
#include <boost/make_unique.hpp>
#include <memory>
#include <fstream>
#include "TelemetryDefinitions.h"

namespace opt = boost::program_options;

const boost::filesystem::path Scheduler::DEFAULT_FILE = "contactPlan.json";
static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::scheduler;

bool contactPlan_t::operator<(const contactPlan_t& o) const {
    if (contact == o.contact) {
        if (source == o.source) {
            if (dest == o.dest) {
                if (finalDest == o.finalDest) {
                    return (start < o.start);
                }
                return (finalDest < o.finalDest);
            }
            return (dest < o.dest);
        }
        return (source < o.source);
    }
    return (contact < o.contact);
}

bool contact_t::operator<(const contact_t& o) const {
    if (source == o.source) {
        return (dest < o.dest);
    }
    return (source < o.source);
}

Scheduler::Scheduler() : 
    m_contactPlanTimer(m_ioService)
{
    
}

void Scheduler::Stop() {
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

Scheduler::~Scheduler() {
    Stop();
}

void Scheduler::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false;
}

bool Scheduler::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //Scope to ensure clean exit before return
    {
        running = false;
        Stop();
        running = true;
        m_runningFromSigHandler = true;
        m_egressFullyInitialized = false;
        m_numOutductCapabilityTelemetriesReceived = 0;

        SignalHandler sigHandler(boost::bind(&Scheduler::MonitorExitKeypressThreadFunction, this));
        HdtnConfig_ptr hdtnConfig;
        opt::options_description desc("Allowed options");

        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("use-unix-timestamp", "Use unix timestamp in contact plan.") 
		("hdtn-config-file", opt::value<boost::filesystem::path>()->default_value("hdtn.json"), "HDTN Configuration File.")
                ("contact-plan-file", opt::value<boost::filesystem::path>()->default_value(Scheduler::DEFAULT_FILE), "Contact Plan file that scheudler relies on for link availability.");

            opt::variables_map vm;
            opt::store(opt::parse_command_line(argc, argv, desc, opt::command_line_style::unix_style | opt::command_line_style::case_insensitive), vm);
            opt::notify(vm);

            if (vm.count("help")) {
                LOG_INFO(subprocess) << desc;
                return false;
            }

	    using_unix_timestamp = (vm.count("use-unix-timestamp") != 0);

            const boost::filesystem::path configFileName = vm["hdtn-config-file"].as<boost::filesystem::path>();

            if(HdtnConfig_ptr ptrConfig = HdtnConfig::CreateFromJsonFilePath(configFileName)) {
                m_hdtnConfig = *ptrConfig;
            }
            else {
                LOG_ERROR(subprocess) << "error loading config file: " << configFileName;
                return false;
            }

            //int src = m_hdtnConfig.m_myNodeId;

            m_contactsFile = vm["contact-plan-file"].as<boost::filesystem::path>();
            if (m_contactsFile.empty()) {
                LOG_INFO(subprocess) << desc;
                return false;
            }

            if (!boost::filesystem::exists(m_contactsFile)) { //first see if the user specified an already valid path name not dependent on HDTN's source root
                m_contactsFile = Scheduler::GetFullyQualifiedFilename(m_contactsFile);
                if (!boost::filesystem::exists(m_contactsFile)) {
                    LOG_ERROR(subprocess) << "ContactPlan File not found: " << m_contactsFile;
                    return false;
                }
            }

            LOG_INFO(subprocess) << "ContactPlan file: " << m_contactsFile;
        }

        catch (boost::bad_any_cast & e) {
            LOG_ERROR(subprocess) << "invalid data error: " << e.what();
            LOG_ERROR(subprocess) << desc;
            return false;
        }
        catch (std::exception& e) {
            LOG_ERROR(subprocess) << "error: " << e.what();
            return false;
        }
        catch (...) {
             LOG_ERROR(subprocess) << "Exception of unknown type!";
             return false;
        }

        LOG_INFO(subprocess) << "starting Scheduler..";

        m_ioService.reset();
        m_workPtr = boost::make_unique<boost::asio::io_service::work>(m_ioService);
        m_contactPlanTimerIsRunning = false;
        m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

 
        //socket for receiving events from Egress
        m_zmqCtxPtr = boost::make_unique<zmq::context_t>();
        m_zmqPullSock_boundEgressToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
        const std::string connect_connectingEgressToBoundSchedulerPath(
        std::string("tcp://") +
        m_hdtnConfig.m_zmqEgressAddress +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressToBoundSchedulerPortPath));
        try {
            m_zmqPullSock_boundEgressToConnectingSchedulerPtr->connect(connect_connectingEgressToBoundSchedulerPath);
            LOG_INFO(subprocess) << "Scheduler connected and listening to events from Egress " << connect_connectingEgressToBoundSchedulerPath;
        }
        catch (const zmq::error_t & ex) {
            LOG_ERROR(subprocess) << "error: scheduler cannot connect to egress socket: " << ex.what();
            return false;
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

        // Socket for sending events to Ingress and Storage
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

        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Scheduler::ReadZmqAcksThreadFunc, this, boost::ref(running))); //create and start the worker thread

        boost::this_thread::sleep(boost::posix_time::seconds(2));

        //Wait until egress up and running and get the first outduct capabilities telemetry.
        //The m_threadZmqAckReaderPtr will call ProcessContactsFile to complete initialization once egress telemetry received for the first time

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        Stop();

        boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();

        LOG_INFO(subprocess) << "Scheduler currentTime  " << timeLocal;
    }
    LOG_INFO(subprocess) << "Scheduler exited cleanly..";
    return true;
}

void Scheduler::SendLinkDown(uint64_t src, uint64_t dest, uint64_t outductArrayIndex,
		             uint64_t time, uint64_t cid) {

    hdtn::IreleaseChangeHdr stopMsg;

    memset(&stopMsg, 0, sizeof(stopMsg));
    memset(&stopMsg.subscriptionBytes, 'a', sizeof(stopMsg.subscriptionBytes));
    stopMsg.base.type = HDTN_MSGTYPE_ILINKDOWN;
    stopMsg.nextHopNodeId = dest;
    stopMsg.prevHopNodeId = src;
    stopMsg.outductArrayIndex = outductArrayIndex;
    stopMsg.time = time;
    stopMsg.contact = cid;

    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->send(zmq::const_buffer(&stopMsg,
            sizeof(stopMsg)), zmq::send_flags::none);
    }
    

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(subprocess) << " -- LINK DOWN Event sent for outductArrayIndex=" << outductArrayIndex
        << "  src(" << src << ") == = > dest(" << dest << ") at time " << timeLocal;
}

void Scheduler::SendLinkUp(uint64_t src, uint64_t dest, uint64_t outductArrayIndex, uint64_t time) {

    hdtn::IreleaseChangeHdr releaseMsg;
    memset(&releaseMsg, 0, sizeof(releaseMsg));
    memset(&releaseMsg.subscriptionBytes, 'a', sizeof(releaseMsg.subscriptionBytes));
    releaseMsg.base.type = HDTN_MSGTYPE_ILINKUP;
    releaseMsg.nextHopNodeId = dest;
    releaseMsg.prevHopNodeId = src;
    releaseMsg.outductArrayIndex = outductArrayIndex;
    releaseMsg.time = time;
    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->send(zmq::const_buffer(&releaseMsg, sizeof(releaseMsg)),
            zmq::send_flags::none);
    }

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(subprocess) << " -- LINK UP Event sent for outductArrayIndex=" << outductArrayIndex
        << "  src(" << src << ") == = > dest(" << dest << ") at time " << timeLocal;
}

void Scheduler::EgressEventsHandler() {
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
        const uint64_t event = linkStatusHdr.event;
        const uint64_t outductArrayIndex = linkStatusHdr.uuid;
        const uint64_t timeSecondsSinceSchedulerEpoch = linkStatusHdr.unixTimeSecondsSince1970 - m_subtractMeFromUnixTimeSecondsToConvertToSchedulerTimeSeconds;

        LOG_INFO(subprocess) << "Received link status event " << event << " from Egress for outductArrayIndex " << outductArrayIndex;

        uint64_t nextHopNodeId;
        {
            boost::mutex::scoped_lock lock(m_mutexFinalDestsToOutductArrayIndexMaps);
            std::map<uint64_t, uint64_t>::const_iterator it = m_mapOutductArrayIndexToNextHopNodeId.find(outductArrayIndex);
            if (it == m_mapOutductArrayIndexToNextHopNodeId.cend()) {
                LOG_ERROR(subprocess) << "EgressEventsHandler got event for unknown outductArrayIndex " << outductArrayIndex << " which does not correspont to a next hop";
                return;
            }
            nextHopNodeId = it->second;
        }

        const uint64_t srcNode = m_hdtnConfig.m_myNodeId;
        const uint64_t destNode = nextHopNodeId;

        LOG_INFO(subprocess) << "EgressEventsHandler nextHopNodeId " << destNode << " and srcNode " << srcNode;
        
	    contact_t contact;
        contact.source = srcNode;
        contact.dest = destNode;

        if (event == 1) {
            bool contactIsUp;
            {
                boost::mutex::scoped_lock lock(m_contactUpSetMutex);
                std::map<contact_t, bool>::const_iterator it = m_mapContactUp.find(contact);
                if (it == m_mapContactUp.cend()) {
                    LOG_ERROR(subprocess) << "EgressEventsHandler got Link Up event for unknown contact src=" << contact.source << " dest=" << contact.dest;
                    return;
                }
                contactIsUp = it->second;
            }
            if (contactIsUp) {
	            LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Up event ";
                SendLinkUp(srcNode, destNode, outductArrayIndex, timeSecondsSinceSchedulerEpoch);
            }
        }
        else {
            LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Down event ";
            SendLinkDown(srcNode, destNode, outductArrayIndex, timeSecondsSinceSchedulerEpoch, 1);
        }
    }
    else if (linkStatusHdr.base.type == HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY) {
        AllOutductCapabilitiesTelemetry_t aoct;
        uint64_t numBytesTakenToDecode;

        zmq::message_t zmqMessageOutductTelem;
        //message guaranteed to be there due to the zmq::send_flags::sndmore
        if (!m_zmqPullSock_boundEgressToConnectingSchedulerPtr->recv(zmqMessageOutductTelem, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "error receiving AllOutductCapabilitiesTelemetry";
        }
        else if (!aoct.DeserializeFromLittleEndian((uint8_t*)zmqMessageOutductTelem.data(), numBytesTakenToDecode, zmqMessageOutductTelem.size())) {
            LOG_ERROR(subprocess) << "error deserializing AllOutductCapabilitiesTelemetry";
        }
        else {
            LOG_DEBUG(subprocess) << "Received Telemetry message from Egress" << aoct;

            boost::mutex::scoped_lock lock(m_mutexFinalDestsToOutductArrayIndexMaps);
            m_mapOutductArrayIndexToNextHopNodeId.clear();
            m_mapNextHopNodeIdToOutductArrayIndex.clear();
            m_mapFinalDestNodeIdToOutductArrayIndex.clear();
            m_mapFinalDestEidToOutductArrayIndex.clear();

            for (std::list<OutductCapabilityTelemetry_t>::const_iterator itAoct = aoct.outductCapabilityTelemetryList.cbegin(); itAoct != aoct.outductCapabilityTelemetryList.cend(); ++itAoct) {
                const OutductCapabilityTelemetry_t& oct = *itAoct;
                m_mapNextHopNodeIdToOutductArrayIndex[oct.nextHopNodeId] = oct.outductArrayIndex;
                m_mapOutductArrayIndexToNextHopNodeId[oct.outductArrayIndex] = oct.nextHopNodeId;
                for (std::list<cbhe_eid_t>::const_iterator it = oct.finalDestinationEidList.cbegin(); it != oct.finalDestinationEidList.cend(); ++it) {
                    const cbhe_eid_t& eid = *it;
                    m_mapFinalDestEidToOutductArrayIndex[eid] = oct.outductArrayIndex;
                }
                for (std::list<uint64_t>::const_iterator it = oct.finalDestinationNodeIdList.cbegin(); it != oct.finalDestinationNodeIdList.cend(); ++it) {
                    const uint64_t nodeId = *it;
                    m_mapFinalDestNodeIdToOutductArrayIndex[nodeId] = oct.outductArrayIndex;
                }
            }
            ++m_numOutductCapabilityTelemetriesReceived;
        }
    }
}

void Scheduler::UisEventsHandler() {
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
        boost::asio::post(m_ioService, boost::bind(&Scheduler::ProcessContactsPtPtr, this, std::move(ptPtr), using_unix_timestamp));
        LOG_INFO(subprocess) << "received Reload contact Plan event with data " << (char*)message.data();
        LOG_INFO(subprocess) << "using unix timestamp " << using_unix_timestamp;
    }
    else {
        LOG_ERROR(subprocess) << "error in Scheduler::UisEventsHandler: unknown hdr " << hdr.base.type;
    }

    
}

void Scheduler::ReadZmqAcksThreadFunc(volatile bool & running) {

    static constexpr unsigned int NUM_SOCKETS = 3;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_boundEgressToConnectingSchedulerPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundUisToConnectingSchedulerPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;
    bool ingressSubscribed = false;
    bool storageSubscribed = false;

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;

    while (running && m_runningFromSigHandler) { //keep thread alive if running
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
                //Ingress unique subscription shall be "a"
                //Storage unique subscription shall be "aa"
                if (!m_zmqXPubSock_boundSchedulerToConnectingSubsPtr->recv(zmqSubscriberDataReceived, zmq::recv_flags::none)) {
                    LOG_ERROR(subprocess) << "subscriber message not received";
                }
                else {
                    const uint8_t* const dataSubscriber = (const uint8_t*)zmqSubscriberDataReceived.data();
                    if ((zmqSubscriberDataReceived.size() == 2) && (dataSubscriber[1] == 'a')) {
                        ingressSubscribed = (dataSubscriber[0] == 0x1);
                        LOG_INFO(subprocess) << "Ingress " << ((ingressSubscribed) ? "subscribed" : "desubscribed");
                    }
                    else if ((zmqSubscriberDataReceived.size() == 3) && (dataSubscriber[1] == 'a') && (dataSubscriber[2] == 'a')) {
                        storageSubscribed = (dataSubscriber[0] == 0x1);
                        LOG_INFO(subprocess) << "Storage " << ((storageSubscribed) ? "subscribed" : "desubscribed");
                    }
                    else {
                        LOG_ERROR(subprocess) << "invalid subscriber message received: length=" << zmqSubscriberDataReceived.size();
                    }
                }
            }

            if ((!m_egressFullyInitialized) && (ingressSubscribed) && (storageSubscribed) && (m_numOutductCapabilityTelemetriesReceived)) {
                //first time this outduct capabilities telemetry received, start remaining scheduler threads
                m_egressFullyInitialized = true;
                LOG_INFO(subprocess) << "Now running and fully initialized and connected to egress.. reading contact file " << m_contactsFile;
                ProcessContactsFile(m_contactsFile, false); //false => don't use unix timestamps
            }
        }
    }
}

bool Scheduler::ProcessContactsPtPtr(std::shared_ptr<boost::property_tree::ptree>& contactsPtPtr, bool useUnixTimestamps) {
    return ProcessContacts(*contactsPtPtr, useUnixTimestamps);
}
bool Scheduler::ProcessContactsJsonText(char * jsonText, bool useUnixTimestamps) {
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonCharArray(jsonText, strlen(jsonText), pt)) {
        return false;
    }
    return ProcessContacts(pt, useUnixTimestamps);
}
bool Scheduler::ProcessContactsJsonText(const std::string& jsonText, bool useUnixTimestamps) {
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonString(jsonText, pt)) {
        return false;
    }
    return ProcessContacts(pt, useUnixTimestamps);
}
bool Scheduler::ProcessContactsFile(const boost::filesystem::path& jsonEventFilePath, bool useUnixTimestamps) {
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonFilePath(jsonEventFilePath, pt)) {
        return false;
    }
    return ProcessContacts(pt, useUnixTimestamps);
}

bool Scheduler::ProcessContacts(const boost::property_tree::ptree& pt, bool useUnixTimestamps) {
    

    m_contactPlanTimer.cancel(); //cancel any running contacts in the timer

    //cancel any existing contacts (make them all link down) (ignore link up) in preparation for new contact plan
    for (ptime_to_contactplan_bimap_t::left_iterator it = m_ptimeToContactPlanBimap.left.begin(); it != m_ptimeToContactPlanBimap.left.end(); ++it) {
        const contactplan_islinkup_pair_t& contactPlan = it->second;
        const bool isLinkUp = contactPlan.second;
        contact_t contact;
        contact.source = contactPlan.first.source;
        contact.dest = contactPlan.first.dest;


        if (!isLinkUp) {
            m_contactUpSetMutex.lock();
            m_mapContactUp[contact] = false;
            m_contactUpSetMutex.unlock();
            LOG_INFO(subprocess) << "m_mapContactUp " << false << " for source " << contact.source << " destination " << contact.dest;
            SendLinkDown(contactPlan.first.source, contactPlan.first.dest, contactPlan.first.finalDest, 
                contactPlan.first.end + 1, contactPlan.first.contact);
        }
    }

    m_ptimeToContactPlanBimap.clear(); //clear the map

    if (useUnixTimestamps) {
        LOG_INFO(subprocess) << "***Using unix timestamp!";
        m_epoch = TimestampUtil::GetUnixEpoch();
        m_subtractMeFromUnixTimeSecondsToConvertToSchedulerTimeSeconds = 0;
    }
    else {
        LOG_INFO(subprocess) << "using now as epoch";
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
        if (!AddContact_NotThreadSafe(linkEvent)) {
            LOG_WARNING(subprocess) << "failed to add a contact";
        }
    }

    LOG_INFO(subprocess) << "Epoch Time:  " << m_epoch;

    m_contactPlanTimerIsRunning = false;
    TryRestartContactPlanTimer(); //wait for next event (do this after all sockets initialized)

    return true;
}


//restarts the timer if it is not running
void Scheduler::TryRestartContactPlanTimer() {
    if (!m_contactPlanTimerIsRunning) {
        ptime_to_contactplan_bimap_t::left_iterator it = m_ptimeToContactPlanBimap.left.begin(); //get first event expiring
        if (it != m_ptimeToContactPlanBimap.left.end()) {
            const boost::posix_time::ptime& expiry = it->first.first;
            m_contactPlanTimer.expires_at(expiry);
            m_contactPlanTimer.async_wait(boost::bind(&Scheduler::OnContactPlan_TimerExpired, this, boost::asio::placeholders::error));
            m_contactPlanTimerIsRunning = true;
        }
        else {
            LOG_INFO(subprocess) << "End of ProcessEventFile";
        }
    }
}

void Scheduler::OnContactPlan_TimerExpired(const boost::system::error_code& e) {
    m_contactPlanTimerIsRunning = false;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        ptime_to_contactplan_bimap_t::left_iterator it = m_ptimeToContactPlanBimap.left.begin(); //get event that started the timer
        if (it != m_ptimeToContactPlanBimap.left.end()) {
            const contactplan_islinkup_pair_t& contactPlanPlusIsLinkUpPair = it->second;
            const contactPlan_t& contactPlan = contactPlanPlusIsLinkUpPair.first;
            const bool isLinkUp = contactPlanPlusIsLinkUpPair.second;
            contact_t contact;
            contact.source = contactPlan.source;
            contact.dest = contactPlan.dest;

            m_contactUpSetMutex.lock();
            m_mapContactUp[contact] = isLinkUp;
            m_contactUpSetMutex.unlock();

            uint64_t outductArrayIndex;
            bool didFindOutductArrayIndex;
            {
                boost::mutex::scoped_lock lock(m_mutexFinalDestsToOutductArrayIndexMaps);
                std::map<uint64_t, uint64_t>::const_iterator itNextHopNodeIdToOutductArrayIndex = m_mapNextHopNodeIdToOutductArrayIndex.find(contact.dest);
                if (itNextHopNodeIdToOutductArrayIndex == m_mapNextHopNodeIdToOutductArrayIndex.cend()) {
                    didFindOutductArrayIndex = false;
                }
                else {
                    didFindOutductArrayIndex = true;
                    outductArrayIndex = itNextHopNodeIdToOutductArrayIndex->second;
                }
            }
            if (didFindOutductArrayIndex) {
                LOG_INFO(subprocess) << "m_mapContactUp " << isLinkUp << " for source " << contact.source << " destination " << contact.dest;

                if (isLinkUp) {
                    SendLinkUp(contactPlan.source, contactPlan.dest, outductArrayIndex, contactPlan.start);
                }
                else {
                    SendLinkDown(contactPlan.source, contactPlan.dest, outductArrayIndex,
                        contactPlan.end + 1, contactPlan.contact);
                }
            }
            else {
                LOG_ERROR(subprocess) << "OnContactPlan_TimerExpired cannot find next hop node id " << contact.dest;
            }
            m_ptimeToContactPlanBimap.left.erase(it);
            TryRestartContactPlanTimer(); //wait for next event
        }
    }
}

bool Scheduler::AddContact_NotThreadSafe(const contactPlan_t& contact) {
    {
        ptime_index_pair_t pipStart(m_epoch + boost::posix_time::seconds(contact.start), 0);
        while (m_ptimeToContactPlanBimap.left.count(pipStart)) {
            pipStart.second += 1; //in case of events that occur at the same time
        }
        contactplan_islinkup_pair_t contactUp(contact, true); //true => add link up event
        if (!m_ptimeToContactPlanBimap.insert(ptime_to_contactplan_bimap_t::value_type(pipStart, contactUp)).second) {
            return false;
        }
    }
    {
        ptime_index_pair_t pipEnd(m_epoch + boost::posix_time::seconds(contact.end), 0);
        while (m_ptimeToContactPlanBimap.left.count(pipEnd)) {
            pipEnd.second += 1; //in case of events that occur at the same time
        }
        contactplan_islinkup_pair_t contactDown(contact, false); //false => add link down event
        if (!m_ptimeToContactPlanBimap.insert(ptime_to_contactplan_bimap_t::value_type(pipEnd, contactDown)).second) {
            return false;
        }
    }
    return true;
}
