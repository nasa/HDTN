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
#include <fstream>
#include <iostream>
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
    std::cout << "Keyboard Interrupt.. exiting\n";
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
                std::cout << desc << "\n";
                return false;
            }

            const std::string configFileName = vm["hdtn-config-file"].as<std::string>();

            if(HdtnConfig_ptr ptrConfig = HdtnConfig::CreateFromJsonFile(configFileName)) {
                m_hdtnConfig = *ptrConfig;
            }
            else {
                std::cerr << "error loading config file: " << configFileName << std::endl;
                return false;
            }

            //int src = m_hdtnConfig.m_myNodeId;

            contactsFile = vm["contact-plan-file"].as<std::string>();
            if (contactsFile.length() < 1) {
                std::cout << desc << "\n";
                return false;
            }

            if (!boost::filesystem::exists(contactsFile)) { //first see if the user specified an already valid path name not dependent on HDTN's source root
                contactsFile = Scheduler::GetFullyQualifiedFilename(contactsFile);
                if (!boost::filesystem::exists(contactsFile)) {
                    std::cerr << "ContactPlan File not found: " << contactsFile << std::endl << std::flush;
                    return false;
                }
            }

            std::cout << "ContactPlan file: " << contactsFile << std::endl;

            const std::string myFinalDestUriEid = vm["dest-uri-eid"].as<std::string>();
            if (!Uri::ParseIpnUriString(myFinalDestUriEid, finalDestEid.nodeId, finalDestEid.serviceId)) {
                std::cerr << "error: bad dest uri string: " << myFinalDestUriEid << std::endl;
                return false;
            }

            finalDestAddr = vm["dest-addr"].as<std::string>();
        }

        catch (boost::bad_any_cast & e) {
            std::cout << "invalid data error: " << e.what() << "\n\n";
            std::cout << desc << "\n";
            return false;
        }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return false;
        }
        catch (...) {
             std::cerr << "Exception of unknown type!\n";
             return false;
        }

        std::cout << "starting Scheduler.." << std::endl;

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
            std::cout << "Scheduler connected and listening to events from Egress " << connect_boundEgressPubSubPath << std::endl;
        }
        catch (const zmq::error_t & ex) {
            std::cerr << "error: scheduler cannot connect to egress socket: " << ex.what() << std::endl;
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
            std::cout << "Scheduler connected and listening to events from UIS " << connect_boundUisPubSubPath << std::endl;
        }
        catch (const zmq::error_t& ex) {
            std::cerr << "error: scheduler cannot connect to UIS socket: " << ex.what() << std::endl;
            return false;
        }

        std::cout << "Scheduler up and running" << std::endl;

        // Socket for sending events to Ingress and Storage
        m_zmqPubSock_boundSchedulerToConnectingSubsPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pub);
        const std::string bind_boundSchedulerPubSubPath(
        std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));

        try {
            m_zmqPubSock_boundSchedulerToConnectingSubsPtr->bind(bind_boundSchedulerPubSubPath);
            std::cout << "[Scheduler] socket bound successfully to " << bind_boundSchedulerPubSubPath << std::endl;

        }
        catch (const zmq::error_t & ex) {
            std::cerr << "[Scheduler] socket failed to bind: " << ex.what() << std::endl;
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

        std::cout << "Scheduler currentTime  " << timeLocal << std::endl << std::flush;
    }
    std::cout << "Scheduler exited cleanly..\n";
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
    

    std::cout << " -- LINK DOWN Event sent for Link " << src << " ===> " << dest << std::endl;
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

    std::cout << " -- LINK UP Event sent for Link " << src << " ===> " << dest << std::endl;
}

void Scheduler::EgressEventsHandler() {
    //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
    static constexpr std::size_t minBufSizeBytes = sizeof(uint64_t) + sizeof(hdtn::LinkStatusHdr);
    m_egressRxBufPtrToStdVec64.resize(minBufSizeBytes / sizeof(uint64_t));
    uint64_t* rxBufRawPtrAlign64 = &m_egressRxBufPtrToStdVec64[0];
    const zmq::recv_buffer_result_t res = m_zmqSubSock_boundEgressToConnectingSchedulerPtr->recv(zmq::mutable_buffer(rxBufRawPtrAlign64, minBufSizeBytes), zmq::recv_flags::none);
    if (!res) {
        std::cerr << "[Scheduler::EgressEventHandler] message not received" << std::endl;
        return;
    }
    else if (res->size < sizeof(hdtn::CommonHdr)) {
        std::cerr << "[Scheduler::EgressEventHandler] res->size < sizeof(hdtn::CommonHdr)" << std::endl;
        return;
    }

    hdtn::CommonHdr* common = (hdtn::CommonHdr*)rxBufRawPtrAlign64;

    if (common->type == HDTN_MSGTYPE_LINKSTATUS) {
        hdtn::LinkStatusHdr* linkStatusMsg = (hdtn::LinkStatusHdr*)rxBufRawPtrAlign64;
        if (res->size != sizeof(hdtn::LinkStatusHdr)) {
            std::cerr << "[Scheduler] EgressEventHandler res->size != sizeof(hdtn::LinkStatusHdr" << std::endl;
            return;
        }
        uint64_t event = linkStatusMsg->event;
        uint64_t outductId = linkStatusMsg->uuid;

        std::cout << "[Scheduler] Received link status event " << event << " from Egress for outduct id " << outductId << std::endl;

        const outduct_element_config_t& thisOutductConfig = m_hdtnConfig.m_outductsConfig.m_outductElementConfigVector[outductId];


        const uint64_t srcNode = m_hdtnConfig.m_myNodeId;
        const uint64_t destNode = thisOutductConfig.nextHopNodeId;

        std::cout << "[Scheduler] EgressEventsHandler nextHopNodeId " << thisOutductConfig.nextHopNodeId << " and srcNode " << srcNode << std::endl;
        for (std::set<std::string>::const_iterator itDestUri = thisOutductConfig.finalDestinationEidUris.cbegin();
            itDestUri != thisOutductConfig.finalDestinationEidUris.cend(); ++itDestUri) {
            const std::string& finalDestinationEidUri = *itDestUri;
            cbhe_eid_t finalDestEid;
            std::cout << "[Scheduler] EgressEventsHandler finalDestinationEidUri " << finalDestinationEidUri << std::endl;
            bool serviceNumberIsWildCard;
            if (!Uri::ParseIpnUriString(finalDestinationEidUri, finalDestEid.nodeId, finalDestEid.serviceId, &serviceNumberIsWildCard)) {
                std::cerr << "error in EgressEventsHandler finalDestinationEidUri " <<
                    finalDestinationEidUri << " is invalid." << std::endl;
                return;
            }
            if (event == 1) {
                std::cout << "[Scheduler] EgressEventsHandler Sending Link Up event " << std::endl;
                SendLinkUp(srcNode, destNode, finalDestEid.nodeId);
            }
            else {
                std::cout << "[Scheduler] EgressEventsHandler Sending Link Down event " << std::endl;
                SendLinkDown(srcNode, destNode, finalDestEid.nodeId);
            }
        }
    }
}

void Scheduler::UisEventsHandler() {
    hdtn::ContactPlanReloadHdr hdr;
    const zmq::recv_buffer_result_t res = m_zmqSubSock_boundUisToConnectingSchedulerPtr->recv(zmq::mutable_buffer(&hdr, sizeof(hdr)), zmq::recv_flags::none);
    if (!res) {
        std::cerr << "error in Scheduler::UisEventsHandler: cannot read hdr" << std::endl;
    }
    else if ((res->truncated()) || (res->size != sizeof(hdr))) {
        std::cerr << "UisEventsHandler hdr message mismatch: untruncated = " << res->untruncated_size
            << " truncated = " << res->size << " expected = " << sizeof(hdtn::ToEgressHdr) << std::endl;
    }
    else if (hdr.base.type == CPM_NEW_CONTACT_PLAN) {
        zmq::message_t message;
        if (!m_zmqSubSock_boundUisToConnectingSchedulerPtr->recv(message, zmq::recv_flags::none)) {
            std::cerr << "[Scheduler::UisEventsHandler] message not received" << std::endl;
            return;
        }
        std::shared_ptr<boost::property_tree::ptree> ptPtr = std::make_shared<boost::property_tree::ptree>(JsonSerializable::GetPropertyTreeFromCharArray((char*)message.data(), message.size()));
        boost::asio::post(m_ioService, boost::bind(&Scheduler::ProcessContactsPtPtr, this, std::move(ptPtr), hdr.using_unix_timestamp));
       std::cout << "[Scheduler] received Reload contact Plan event with data " << (char*)message.data() << std::endl;
    }
    else {
        std::cerr << "error in Scheduler::UisEventsHandler: unknown hdr " << hdr.base.type << std::endl;
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
            std::cout << "caught zmq::error_t in Ingress::ReadZmqAcksThreadFunc: " << e.what() << std::endl;
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
        std::cout << "***Using unix timestamp!" << std::endl;    
        m_epoch = boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1));
    }
    else {
	std::cout << "using now as epoch" << std::endl; 
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
            std::cout << "failed to add a contact\n";
        }
    }

    std::cout << "Epoch Time:  " << m_epoch << std::endl << std::flush;

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
            std::cout << "End of ProcessEventFile\n";
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
    else {
        //std::cout << "timer cancelled\n";
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
