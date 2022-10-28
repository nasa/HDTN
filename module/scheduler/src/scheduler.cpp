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

namespace opt = boost::program_options;

const std::string Scheduler::DEFAULT_FILE = "contactPlan.json";
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
        Stop();
        running = true;
        m_runningFromSigHandler = true;
        std::string contactsFile;

        SignalHandler sigHandler(boost::bind(&Scheduler::MonitorExitKeypressThreadFunction, this));
        HdtnConfig_ptr hdtnConfig;
        cbhe_eid_t finalDestEid;
        std::string finalDestAddr;
        opt::options_description desc("Allowed options");

        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("hdtn-config-file", opt::value<std::string>()->default_value("hdtn.json"), "HDTN Configuration File.")
                ("contact-plan-file", opt::value<std::string>()->default_value(Scheduler::DEFAULT_FILE), "Contact Plan file that scheudler relies on for link availability.")
                ("dest-uri-eid", opt::value<std::string>()->default_value("ipn:2.1"), "final destination Eid")
                ("dest-addr", opt::value<std::string>()->default_value("127.0.0.1"), "final destination IP addr to ping for link availability");

            opt::variables_map vm;
            opt::store(opt::parse_command_line(argc, argv, desc, opt::command_line_style::unix_style | opt::command_line_style::case_insensitive), vm);
            opt::notify(vm);

            if (vm.count("help")) {
                LOG_INFO(subprocess) << desc;
                return false;
            }

            const std::string configFileName = vm["hdtn-config-file"].as<std::string>();

            if(HdtnConfig_ptr ptrConfig = HdtnConfig::CreateFromJsonFile(configFileName)) {
                m_hdtnConfig = *ptrConfig;
            }
            else {
                LOG_ERROR(subprocess) << "error loading config file: " << configFileName;
                return false;
            }

            //int src = m_hdtnConfig.m_myNodeId;

            contactsFile = vm["contact-plan-file"].as<std::string>();
            if (contactsFile.length() < 1) {
                LOG_INFO(subprocess) << desc;
                return false;
            }

            if (!boost::filesystem::exists(contactsFile)) { //first see if the user specified an already valid path name not dependent on HDTN's source root
                contactsFile = Scheduler::GetFullyQualifiedFilename(contactsFile);
                if (!boost::filesystem::exists(contactsFile)) {
                    LOG_ERROR(subprocess) << "ContactPlan File not found: " << contactsFile;
                    return false;
                }
            }

            LOG_INFO(subprocess) << "ContactPlan file: " << contactsFile;

            const std::string myFinalDestUriEid = vm["dest-uri-eid"].as<std::string>();
            if (!Uri::ParseIpnUriString(myFinalDestUriEid, finalDestEid.nodeId, finalDestEid.serviceId)) {
                LOG_ERROR(subprocess) << "error: bad dest uri string: " << myFinalDestUriEid;
                return false;
            }

            finalDestAddr = vm["dest-addr"].as<std::string>();
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
        m_zmqSubSock_boundEgressToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::sub);
        const std::string connect_boundEgressPubSubPath(
        std::string("tcp://") +
        m_hdtnConfig.m_zmqEgressAddress +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressToBoundSchedulerPortPath));
        try {
            m_zmqSubSock_boundEgressToConnectingSchedulerPtr->connect(connect_boundEgressPubSubPath);
            m_zmqSubSock_boundEgressToConnectingSchedulerPtr->set(zmq::sockopt::subscribe, "");
            LOG_INFO(subprocess) << "Scheduler connected and listening to events from Egress " << connect_boundEgressPubSubPath;
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
        m_zmqPubSock_boundSchedulerToConnectingSubsPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pub);
        const std::string bind_boundSchedulerPubSubPath(
        std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));

        try {
            m_zmqPubSock_boundSchedulerToConnectingSubsPtr->bind(bind_boundSchedulerPubSubPath);
            LOG_INFO(subprocess) << "socket bound successfully to " << bind_boundSchedulerPubSubPath;

        }
        catch (const zmq::error_t & ex) {
            LOG_ERROR(subprocess) << "socket failed to bind: " << ex.what();
            return false;
        }

        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Scheduler::ReadZmqAcksThreadFunc, this, boost::ref(running))); //create and start the worker thread

        boost::this_thread::sleep(boost::posix_time::seconds(2));

        ProcessContactsFile(contactsFile, false); //false => don't use unix timestamps

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

void Scheduler::SendLinkDown(uint64_t src, uint64_t dest, uint64_t finalDestinationNodeId) {

    hdtn::IreleaseStopHdr stopMsg;

    memset(&stopMsg, 0, sizeof(hdtn::IreleaseStopHdr));
    stopMsg.base.type = HDTN_MSGTYPE_ILINKDOWN;
    stopMsg.nextHopNodeId = dest;
    stopMsg.prevHopNodeId = src;
    stopMsg.finalDestinationNodeId = finalDestinationNodeId;
    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        m_zmqPubSock_boundSchedulerToConnectingSubsPtr->send(zmq::const_buffer(&stopMsg,
            sizeof(hdtn::IreleaseStopHdr)), zmq::send_flags::none);
    }
    

    LOG_INFO(subprocess) << " -- LINK DOWN Event sent for Link " << src << " ===> " << dest;
}

void Scheduler::SendLinkUp(uint64_t src, uint64_t dest, uint64_t finalDestinationNodeId) {

    hdtn::IreleaseStartHdr releaseMsg;
    memset(&releaseMsg, 0, sizeof(hdtn::IreleaseStartHdr));

    releaseMsg.base.type = HDTN_MSGTYPE_ILINKUP;
    releaseMsg.nextHopNodeId = dest;
    releaseMsg.prevHopNodeId = src;
    releaseMsg.finalDestinationNodeId = finalDestinationNodeId;
    {
        boost::mutex::scoped_lock lock(m_mutexZmqPubSock);
        m_zmqPubSock_boundSchedulerToConnectingSubsPtr->send(zmq::const_buffer(&releaseMsg, sizeof(hdtn::IreleaseStartHdr)),
            zmq::send_flags::none);
    }

    LOG_INFO(subprocess) << " -- LINK UP Event sent for Link " << src << " ===> " << dest;
}

void Scheduler::EgressEventsHandler() {
    //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
    static constexpr std::size_t minBufSizeBytes = sizeof(uint64_t) + sizeof(hdtn::LinkStatusHdr);
    m_egressRxBufPtrToStdVec64.resize(minBufSizeBytes / sizeof(uint64_t));
    uint64_t* rxBufRawPtrAlign64 = &m_egressRxBufPtrToStdVec64[0];
    const zmq::recv_buffer_result_t res = m_zmqSubSock_boundEgressToConnectingSchedulerPtr->recv(zmq::mutable_buffer(rxBufRawPtrAlign64, minBufSizeBytes), zmq::recv_flags::none);
    if (!res) {
        LOG_ERROR(subprocess) << "[EgressEventHandler] message not received";
        return;
    }
    else if (res->size < sizeof(hdtn::CommonHdr)) {
        LOG_ERROR(subprocess) << "[EgressEventHandler] res->size < sizeof(hdtn::CommonHdr)";
        return;
    }

    hdtn::CommonHdr* common = (hdtn::CommonHdr*)rxBufRawPtrAlign64;

    if (common->type == HDTN_MSGTYPE_LINKSTATUS) {
        hdtn::LinkStatusHdr* linkStatusMsg = (hdtn::LinkStatusHdr*)rxBufRawPtrAlign64;
        if (res->size != sizeof(hdtn::LinkStatusHdr)) {
            LOG_ERROR(subprocess) << "EgressEventHandler res->size != sizeof(hdtn::LinkStatusHdr";
            return;
        }
        uint64_t event = linkStatusMsg->event;
        uint64_t outductId = linkStatusMsg->uuid;

        LOG_INFO(subprocess) << "Received link status event " << event << " from Egress for outduct id " << outductId;

        const outduct_element_config_t& thisOutductConfig = m_hdtnConfig.m_outductsConfig.m_outductElementConfigVector[outductId];


        const uint64_t srcNode = m_hdtnConfig.m_myNodeId;
        const uint64_t destNode = thisOutductConfig.nextHopNodeId;

        LOG_INFO(subprocess) << "EgressEventsHandler nextHopNodeId " << thisOutductConfig.nextHopNodeId << " and srcNode " << srcNode;
        for (std::set<std::string>::const_iterator itDestUri = thisOutductConfig.finalDestinationEidUris.cbegin();
            itDestUri != thisOutductConfig.finalDestinationEidUris.cend(); ++itDestUri) {
            const std::string& finalDestinationEidUri = *itDestUri;
            cbhe_eid_t finalDestEid;
            LOG_INFO(subprocess) << "EgressEventsHandler finalDestinationEidUri " << finalDestinationEidUri;
            bool serviceNumberIsWildCard;
            if (!Uri::ParseIpnUriString(finalDestinationEidUri, finalDestEid.nodeId, finalDestEid.serviceId, &serviceNumberIsWildCard)) {
                LOG_ERROR(subprocess) << "error in EgressEventsHandler finalDestinationEidUri " <<
                    finalDestinationEidUri << " is invalid.";
                return;
            }
            if (event == 1) {
                LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Up event ";
                SendLinkUp(srcNode, destNode, finalDestEid.nodeId);
            }
            else {
                LOG_INFO(subprocess) << "EgressEventsHandler Sending Link Down event ";
                SendLinkDown(srcNode, destNode, finalDestEid.nodeId);
            }
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
        std::shared_ptr<boost::property_tree::ptree> ptPtr = std::make_shared<boost::property_tree::ptree>(JsonSerializable::GetPropertyTreeFromCharArray((char*)message.data(), message.size()));
        boost::asio::post(m_ioService, boost::bind(&Scheduler::ProcessContactsPtPtr, this, std::move(ptPtr), hdr.using_unix_timestamp));
       LOG_INFO(subprocess) << "received Reload contact Plan event with data " << (char*)message.data();
    }
    else {
        LOG_ERROR(subprocess) << "error in Scheduler::UisEventsHandler: unknown hdr " << hdr.base.type;
    }

    
}

void Scheduler::ReadZmqAcksThreadFunc(volatile bool & running) {

    static constexpr unsigned int NUM_SOCKETS = 2;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqSubSock_boundEgressToConnectingSchedulerPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundUisToConnectingSchedulerPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;

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
        }
    }
}

int Scheduler::ProcessContactsPtPtr(std::shared_ptr<boost::property_tree::ptree>& contactsPtPtr, bool useUnixTimestamps) {
    return ProcessContacts(*contactsPtPtr, useUnixTimestamps);
}
int Scheduler::ProcessContactsJsonText(char * jsonText, bool useUnixTimestamps) {
    boost::property_tree::ptree pt = JsonSerializable::GetPropertyTreeFromCharArray(jsonText, strlen(jsonText));
    return ProcessContacts(pt, useUnixTimestamps);
}
int Scheduler::ProcessContactsJsonText(const std::string& jsonText, bool useUnixTimestamps) {
    boost::property_tree::ptree pt = JsonSerializable::GetPropertyTreeFromJsonString(jsonText);
    return ProcessContacts(pt, useUnixTimestamps);
}
int Scheduler::ProcessContactsFile(const std::string& jsonEventFileName, bool useUnixTimestamps) {
    boost::property_tree::ptree pt = JsonSerializable::GetPropertyTreeFromJsonFile(jsonEventFileName);
    return ProcessContacts(pt, useUnixTimestamps);
}

int Scheduler::ProcessContacts(const boost::property_tree::ptree& pt, bool useUnixTimestamps) {
    

    m_contactPlanTimer.cancel(); //cancel any running contacts in the timer

    //cancel any existing contacts (make them all link down) (ignore link up) in preparation for new contact plan
    for (ptime_to_contactplan_bimap_t::left_iterator it = m_ptimeToContactPlanBimap.left.begin(); it != m_ptimeToContactPlanBimap.left.end(); ++it) {
        const contactplan_islinkup_pair_t& contactPlan = it->second;
        const bool isLinkUp = contactPlan.second;
        if (!isLinkUp) {
            SendLinkDown(contactPlan.first.source, contactPlan.first.dest, contactPlan.first.finalDest);
        }
    }

    m_ptimeToContactPlanBimap.clear(); //clear the map

    if (useUnixTimestamps) {
        LOG_INFO(subprocess) << "***Using unix timestamp!";
        m_epoch = boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1));
    }
    else {
	LOG_INFO(subprocess) << "using now as epoch";
        m_epoch = boost::posix_time::microsec_clock::universal_time();
    }
    
    const boost::property_tree::ptree& contactsPt = pt.get_child("contacts", boost::property_tree::ptree());
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

    return 0;
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
            const contactplan_islinkup_pair_t& contactPlan = it->second;
            const bool isLinkUp = contactPlan.second;
            if (isLinkUp) {
                SendLinkUp(contactPlan.first.source, contactPlan.first.dest, contactPlan.first.finalDest);
            }
            else {
                SendLinkDown(contactPlan.first.source, contactPlan.first.dest, contactPlan.first.finalDest);
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
