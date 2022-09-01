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
#include <memory>
#include <fstream>

using namespace std;

namespace opt = boost::program_options;

const std::string Scheduler::DEFAULT_FILE = "contactPlan.json";

Scheduler::Scheduler() : 
    m_timersFinished(false) 
{
}

Scheduler::~Scheduler() {
if (m_threadZmqAckReaderPtr) {
        m_threadZmqAckReaderPtr->join();
        m_threadZmqAckReaderPtr.reset(); //delete it
    }

}

void Scheduler::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false;
}

void Scheduler::PingCommand(const boost::system::error_code& e, boost::asio::deadline_timer* dt, const cbhe_eid_t* finalDestEid,
                                   zmq::socket_t * socket, const char* command)
{

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    if (!e) {
        if (system(command) ) {
            std::cout <<   "Ping Failed  ==> Send Link Down Event \n" << std::endl << std::flush;
            std::cout <<  timeLocal << ": Processing Event Link Unavailable for finalDestinationEid: (" << finalDestEid->nodeId << "," << finalDestEid->serviceId << ")" << std::endl;
             hdtn::IreleaseStopHdr stopMsg;
             memset(&stopMsg, 0, sizeof(hdtn::IreleaseStopHdr));
             stopMsg.base.type = HDTN_MSGTYPE_ILINKDOWN;
             stopMsg.finalDestinationEid = *finalDestEid;
             socket->send(zmq::const_buffer(&stopMsg, sizeof(hdtn::IreleaseStopHdr)), zmq::send_flags::none);
             std::cout << " -- LINK DOWN Event sent for destination: (" << finalDestEid->nodeId << "," << finalDestEid->serviceId << ")" << std::endl;

        } else {
            std::cout << "Ping Success ==> Send Link Up Event!  \n" << std::endl << std::flush;
            std::cout << timeLocal << ": Processing Event  Link Available for finalDestinationEid: (" << finalDestEid->nodeId << "," << finalDestEid->serviceId << ")" << std::endl;
            hdtn::IreleaseStartHdr releaseMsg;
            memset(&releaseMsg, 0, sizeof(hdtn::IreleaseStartHdr));
            releaseMsg.base.type = HDTN_MSGTYPE_ILINKUP;
            releaseMsg.finalDestinationEid = *finalDestEid;
            socket->send(zmq::const_buffer(&releaseMsg, sizeof(hdtn::IreleaseStartHdr)), zmq::send_flags::none);
            std::cout << " -- LINK UP Event sent for destination: (" << finalDestEid->nodeId << "," << finalDestEid->serviceId << ")" << std::endl;
        }

        dt->expires_at(dt->expires_at() + boost::posix_time::seconds(5));
        dt->async_wait(boost::bind(Scheduler::PingCommand,
                       boost::asio::placeholders::error,
                       dt, finalDestEid, socket, command));
    } else {
        std::cout << "timer dt cancelled\n";
    }
}

bool Scheduler::Run(int argc, const char* const argv[], volatile bool & running, 
		    std::string jsonEventFileName, bool useSignalHandler) {
    //Scope to ensure clean exit before return
    {
        running = true;
        m_runningFromSigHandler = true;
        m_timersFinished = false;
        jsonEventFileName = "";
        std::string contactsFile = Scheduler::DEFAULT_FILE;

        SignalHandler sigHandler(boost::bind(&Scheduler::MonitorExitKeypressThreadFunction, this));
        HdtnConfig_ptr hdtnConfig;
        cbhe_eid_t finalDestEid;
        string finalDestAddr;
        opt::options_description desc("Allowed options");
        bool isPingTest = false; 

        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("hdtn-config-file", opt::value<std::string>()->default_value("2dtn.json"), "HDTN Configuration File.")
		("contact-plan-file", opt::value<std::string>()->default_value(Scheduler::DEFAULT_FILE),
                "Contact Plan file that scheudler relies on for link availability.")
                ("ping-test", "Scheduler only relies on ping results for link availability.")
                ("dest-uri-eid", opt::value<std::string>()->default_value("ipn:2.1"), "final destination Eid")
                ("dest-addr", opt::value<std::string>()->default_value("127.0.0.1"), "final destination IP addr to ping for link availability")
		;

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
            } else {
                std::cerr << "error loading config file: " << configFileName << std::endl;
                return false;
            }

	    contactsFile = vm["contact-plan-file"].as<std::string>();
            if (contactsFile.length() < 1) {
                std::cout << desc << "\n";
                return false;
            }

           std::string jsonFileName =  Scheduler::GetFullyQualifiedFilename(contactsFile);
           if ( !boost::filesystem::exists( jsonFileName ) ) {
               std::cerr << "ContactPlan File not found: " << jsonFileName << std::endl << std::flush;
               return false;
            }
            
	    jsonEventFileName = jsonFileName;

            std::cout << "ContactPlan file: " << jsonEventFileName << std::endl;
	 
	    if (vm.count("ping-test")) {
                isPingTest = true;
            }

            const std::string myFinalDestUriEid = vm["dest-uri-eid"].as<string>();
            if (!Uri::ParseIpnUriString(myFinalDestUriEid, finalDestEid.nodeId, finalDestEid.serviceId)) {
                std::cerr << "error: bad dest uri string: " << myFinalDestUriEid << std::endl;
                return false;
            }

            finalDestAddr = vm["dest-addr"].as<string>();
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
        } catch (const zmq::error_t & ex) {
            std::cerr << "error: scheduler cannot connect to egress socket: " << ex.what() << std::endl;
            return false;
        }

	Scheduler scheduler;

        std::cout << "Scheduler up and running" << std::endl;

	// Socket for sending events to Ingress and Storage
	zmq::context_t ctx;
        zmq::socket_t socket(ctx, zmq::socket_type::pub);
        const std::string bind_boundSchedulerPubSubPath(
        std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));

        try {
            socket.bind(bind_boundSchedulerPubSubPath);
            std::cout << "[Scheduler] socket bound successfully to " << bind_boundSchedulerPubSubPath << std::endl;

        } catch (const zmq::error_t & ex) {
            std::cerr << "[Scheduler] socket failed to bind: " << ex.what() << std::endl;
            return false;
        }

	m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Scheduler::ReadZmqAcksThreadFunc, this, running, &socket)); //create and start the worker thread


	if (!isPingTest) {
	     scheduler.ProcessContactsFile(&jsonEventFileName, &socket);
        } else {
	    boost::asio::io_service service;
            boost::asio::deadline_timer dt(service, boost::posix_time::seconds(5));
            string str = finalDestAddr;
            str = "ping -c1 -s1 " + str;
            const char *command = str.c_str();
	    dt.async_wait(boost::bind(Scheduler::PingCommand, boost::asio::placeholders::error, &dt, &finalDestEid, &socket, command));
	    service.run();
	}

	if (useSignalHandler) {
            sigHandler.Start(false);
        }
	while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        socket.close();
        m_timersFinished = true;

        boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();

        std::cout << "Scheduler currentTime  " << timeLocal << std::endl << std::flush;
    }
    std::cout << "Scheduler exiting cleanly..\n";
    return true;
}

void Scheduler::SendLinkDown(uint64_t src, uint64_t dest, cbhe_eid_t finalDestinationEid,
                             zmq::socket_t * ptrSocket) {

    hdtn::IreleaseStopHdr stopMsg;
    cbhe_eid_t nextHopEid;
    nextHopEid.nodeId = dest;
    nextHopEid.serviceId = 1;

    cbhe_eid_t prevHopEid;
    prevHopEid.nodeId = src;
    prevHopEid.serviceId = 1;

    memset(&stopMsg, 0, sizeof(hdtn::IreleaseStopHdr));
    stopMsg.base.type = HDTN_MSGTYPE_ILINKDOWN;
    stopMsg.nextHopEid = nextHopEid;
    stopMsg.prevHopEid = prevHopEid;
    stopMsg.finalDestinationEid = finalDestinationEid;
    ptrSocket->send(zmq::const_buffer(&stopMsg,
    sizeof(hdtn::IreleaseStopHdr)), zmq::send_flags::none);

    std::cout << " -- LINK DOWN Event sent for Link " <<
    prevHopEid.nodeId << " ===> " << nextHopEid.nodeId << "" <<  std::endl;
}

void Scheduler::ProcessLinkDown(const boost::system::error_code& e, uint64_t src, uint64_t dest, cbhe_eid_t finalDestinationEid,
		std::string event, zmq::socket_t * ptrSocket) {
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    if (e != boost::asio::error::operation_aborted) {
	 std::cout << timeLocal << ": Processing Event " << event << " from source node " << src << " to next Hop node " << dest << std::endl;
         SendLinkDown(src, dest, finalDestinationEid, ptrSocket);
     } else {
        std::cout << "timer dt2 cancelled\n";
    }
}

void Scheduler::SendLinkUp(uint64_t src, uint64_t dest, cbhe_eid_t finalDestinationEid,
                           zmq::socket_t * ptrSocket) {

    hdtn::IreleaseStartHdr releaseMsg;
    memset(&releaseMsg, 0, sizeof(hdtn::IreleaseStartHdr));
    cbhe_eid_t nextHopEid;
    nextHopEid.nodeId = dest;
    nextHopEid.serviceId = 1;

    cbhe_eid_t prevHopEid;
    prevHopEid.nodeId = src;
    prevHopEid.serviceId = 1;

    releaseMsg.base.type = HDTN_MSGTYPE_ILINKUP;
    releaseMsg.nextHopEid = nextHopEid;
    releaseMsg.prevHopEid = prevHopEid;
    releaseMsg.finalDestinationEid = finalDestinationEid;
    ptrSocket->send(zmq::const_buffer(&releaseMsg, sizeof(hdtn::IreleaseStartHdr)),
                    zmq::send_flags::none);

    std::cout << " -- LINK UP Event sent for Link " <<
    prevHopEid.nodeId << " ===> " << nextHopEid.nodeId << "" <<  std::endl;
}

void Scheduler::ProcessLinkUp(const boost::system::error_code& e, uint64_t src, uint64_t dest, cbhe_eid_t finalDestinationEid,
	       	std::string event, zmq::socket_t * ptrSocket) {
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
	std::cout << timeLocal << ": Processing Event " << event << " from source node " << src << " to next Hop node " << dest << std::endl;
        SendLinkUp(src, dest, finalDestinationEid, ptrSocket);

    } else {
        std::cout << "timer dt cancelled\n";
    }
}

void Scheduler::EgressEventsHandler(zmq::socket_t * socket) {
    //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
    static constexpr std::size_t minBufSizeBytes = sizeof(uint64_t) + sizeof(hdtn::LinkStatusHdr);
    m_egressRxBufPtrToStdVec64.resize(minBufSizeBytes / sizeof(uint64_t));
    uint64_t * rxBufRawPtrAlign64 = &m_egressRxBufPtrToStdVec64[0];
    const zmq::recv_buffer_result_t res = m_zmqSubSock_boundEgressToConnectingSchedulerPtr->recv(zmq::mutable_buffer(rxBufRawPtrAlign64, minBufSizeBytes), zmq::recv_flags::none);
    if (!res) {
        std::cerr << "[Scheduler::EgressEventHandler] message not received" << std::endl;
        return;
    } else if (res->size < sizeof(hdtn::CommonHdr)) {
        std::cerr << "[Scheduler::EgressEventHandler] res->size < sizeof(hdtn::CommonHdr)" << std::endl;
        return;
    }

    hdtn::CommonHdr *common = (hdtn::CommonHdr *)rxBufRawPtrAlign64;

    if (common->type == HDTN_MSGTYPE_LINKSTATUS) {
        hdtn::LinkStatusHdr * linkStatusMsg = (hdtn::LinkStatusHdr *)rxBufRawPtrAlign64;
        if (res->size != sizeof(hdtn::LinkStatusHdr)) {
            std::cerr << "[Scheduler] EgressEventHandler res->size != sizeof(hdtn::LinkStatusHdr" << std::endl;
            return;
        }
	uint64_t event = linkStatusMsg->event;
        uint64_t outductId = linkStatusMsg->uuid;

        std::cout << "[Scheduler] Received link status event " << event <<  " from Egress for outduct id " << outductId << std::endl;
    
        const outduct_element_config_t & thisOutductConfig = m_hdtnConfig.m_outductsConfig.m_outductElementConfigVector[outductId];
        const std::string destUri = thisOutductConfig.nextHopEndpointId;

        cbhe_eid_t destEid;

        if (!Uri::ParseIpnUriString(destUri, destEid.nodeId, destEid.serviceId)) {
            std::cerr << "error in EgressEventsHandler nextHopUri " << destUri << " is invalid." << std::endl;
            return;
        }

	const uint64_t srcNode = m_hdtnConfig.m_myNodeId;
        const uint64_t destNode = destEid.nodeId;

	std::cout << "[Scheduler] EgressEventsHandler nextHopUri " << destUri << " and srcNode " << srcNode <<  std::endl;
        for (std::set<std::string>::const_iterator itDestUri = thisOutductConfig.finalDestinationEidUris.cbegin(); 
            itDestUri != thisOutductConfig.finalDestinationEidUris.cend(); ++itDestUri) {
            const std::string & finalDestinationEidUri = *itDestUri;
            cbhe_eid_t finalDestEid;
            std::cout << "[Scheduler] EgressEventsHandler finalDestinationEidUri " << finalDestinationEidUri <<  std::endl;

	    if (!Uri::ParseIpnUriString(finalDestinationEidUri, finalDestEid.nodeId, finalDestEid.serviceId)) {
                std::cerr << "error in EgressEventsHandler finalDestinationEidUri " << 
                finalDestinationEidUri << " is invalid." << std::endl;     
                return;
	    }
	    if (event == 1) {
		std::cout << "[Scheduler] EgressEventsHandler Sending Link Up event " <<  std::endl;
                SendLinkUp(srcNode, destNode, finalDestEid, socket);
	    } else { 
		std::cout << "[Scheduler] EgressEventsHandler Sending Link Down event " <<  std::endl;
                SendLinkDown(srcNode, destNode, finalDestEid, socket);
	    }
        }
    }
}

void Scheduler::ReadZmqAcksThreadFunc(volatile bool running, zmq::socket_t * socket) {

    static constexpr unsigned int NUM_SOCKETS = 1;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqSubSock_boundEgressToConnectingSchedulerPtr->handle(), 0, ZMQ_POLLIN, 0},
    };
    std::size_t totalAcksFromEgress = 0;

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;

    while (running) { //keep thread alive if running
        int rc = 0;
        try {
            rc = zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
        }
        catch (zmq::error_t & e) {
            std::cout << "caught zmq::error_t in Ingress::ReadZmqAcksThreadFunc: " << e.what() << std::endl;
            continue;
        }
        if (rc > 0) {
            if (items[0].revents & ZMQ_POLLIN) { //events from Egress
                EgressEventsHandler(socket);
            }
        }
    }
}


int Scheduler::ProcessContactsFile(std::string* jsonEventFileName, zmq::socket_t * socket) 
{
    m_timersFinished = false;
    contactPlanVector_t contactsVector;

    boost::property_tree::ptree pt = JsonSerializable::GetPropertyTreeFromJsonFile(*jsonEventFileName);
    const boost::property_tree::ptree & contactsPt
            = pt.get_child("contacts", boost::property_tree::ptree());
    contactsVector.resize(contactsPt.size());
    unsigned int eventIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, contactsPt) {
        contactPlan_t & linkEvent = contactsVector[eventIndex++];
        linkEvent.contact = eventPt.second.get<int>("contact", 0);
        linkEvent.source = eventPt.second.get<int>("source", 0);
        linkEvent.dest = eventPt.second.get<int>("dest", 0);
	linkEvent.finalDest = eventPt.second.get<int>("finalDestination", 0);
        linkEvent.start = eventPt.second.get<int>("startTime", 0);
        linkEvent.end = eventPt.second.get<int>("endTime", 0);
        linkEvent.rate = eventPt.second.get<int>("rate", 0);
    }

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    boost::posix_time::ptime epochTime = boost::posix_time::second_clock::local_time();

    std::cout << "Epoch Time:  " << epochTime << std::endl << std::flush;

    boost::asio::io_service ioService;

    std::vector<SmartDeadlineTimer> vectorTimers;
    vectorTimers.reserve(contactsVector.size());

    std::vector<SmartDeadlineTimer> vectorTimers2;
    vectorTimers2.reserve(contactsVector.size());

    for(std::size_t i=0; i < contactsVector.size(); ++i) {
        SmartDeadlineTimer dt = boost::make_unique<boost::asio::deadline_timer>(ioService);
        SmartDeadlineTimer dt2 = boost::make_unique<boost::asio::deadline_timer>(ioService);
        
        cbhe_eid_t finalDestination;
        finalDestination.nodeId = contactsVector[i].finalDest; 
        finalDestination.serviceId = 1; 

        dt->expires_from_now(boost::posix_time::seconds(contactsVector[i].start));
        dt->async_wait(boost::bind(&Scheduler::ProcessLinkUp,this,boost::asio::placeholders::error, 
				contactsVector[i].source, contactsVector[i].dest,
                                finalDestination, "Link Available", socket));
        vectorTimers.push_back(std::move(dt));

        dt2->expires_from_now(boost::posix_time::seconds(contactsVector[i].end + 1));                     
        dt2->async_wait(boost::bind(&Scheduler::ProcessLinkDown,this,boost::asio::placeholders::error, 
				contactsVector[i].source, 
				contactsVector[i].dest,
        			finalDestination, "Link Unavailable", socket));
        vectorTimers2.push_back(std::move(dt2));
    }

    ioService.run();

    m_timersFinished = true;
    
    timeLocal = boost::posix_time::second_clock::local_time();
    std::cout << "End of ProcessEventFile:  " << timeLocal << std::endl << std::flush;
    
    return 0;
}

int Scheduler::ProcessComandLine(int argc, const char *argv[], std::string& jsonEventFileName) {
    jsonEventFileName = "";
    std::string contactsFile = Scheduler::DEFAULT_FILE;
    boost::program_options::options_description desc("Allowed options");
    try {
        desc.add_options()
            ("help", "Produce help message.")
            ("hdtn-config-file", boost::program_options::value<std::string>()->default_value("hdtn.json"), "HDTN Configuration File.")
            ("contact-plan-file", boost::program_options::value<std::string>()->default_value(Scheduler::DEFAULT_FILE),
             "Contact Plan file.");
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc,
                boost::program_options::command_line_style::unix_style
               | boost::program_options::command_line_style::case_insensitive), vm);
        boost::program_options::notify(vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }
        contactsFile = vm["contact-plan-file"].as<std::string>();
        if (contactsFile.length() < 1) {
            std::cout << desc << "\n";
            return 1;
        }
        const std::string configFileName = vm["hdtn-config-file"].as<std::string>();

        if(HdtnConfig_ptr ptrConfig = HdtnConfig::CreateFromJsonFile(configFileName)) {
            m_hdtnConfig = *ptrConfig;
        }
        else {
            std::cerr << "error loading config file: " << configFileName << std::endl;
            return false;
        }
    }
    catch (std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch (...) {
        std::cerr << "Exception of unknown type!\n";
        return 1;
    }
    std::string jsonFileName =  Scheduler::GetFullyQualifiedFilename(contactsFile);
    if ( !boost::filesystem::exists( jsonFileName ) ) {
        std::cerr << "File not found: " << jsonFileName << std::endl << std::flush;
        return 1;
    }
    jsonEventFileName = jsonFileName;
    return 0;
}
