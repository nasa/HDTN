/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 * Scheduler - sends events on link availability based on contact plan 
 * schedule to storage and ingress
 ****************************************************************************
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
#include <boost/shared_ptr.hpp>
#include <fstream>

using namespace std;

namespace opt = boost::program_options;

const std::string Scheduler::DEFAULT_FILE = "contactPlan.json";

Scheduler::Scheduler() {
    m_timersFinished = false;
}

Scheduler::~Scheduler() {

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

            hdtnConfig = HdtnConfig::CreateFromJsonFile(configFileName);
            if (!hdtnConfig) {
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
	
	Scheduler scheduler;
        if (!isPingTest) {
	     scheduler.ProcessContactsFile(&jsonEventFileName);
             return true;
        }

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "Scheduler up and running" << std::endl;

	zmq::context_t ctx;
        zmq::socket_t socket(ctx, zmq::socket_type::pub);
        const std::string bind_boundSchedulerPubSubPath(
        std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));

        try {
            socket.bind(bind_boundSchedulerPubSubPath);
        } catch (const zmq::error_t & ex) {
            std::cerr << "Scheduler socket failed to bind: " << ex.what() << std::endl;
            return false;
        }
	
	boost::asio::io_service service;
        boost::asio::deadline_timer dt(service, boost::posix_time::seconds(5));

        string str = finalDestAddr;
        str = "ping -c1 -s1 " + str;

        const char *command = str.c_str();
	
	dt.async_wait(boost::bind(Scheduler::PingCommand, boost::asio::placeholders::error, &dt, &finalDestEid, &socket, command));
	service.run();

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



void Scheduler::ProcessLinkDown(const boost::system::error_code& e, int src, int dest, cbhe_eid_t finalDestinationEid, 
		std::string event, zmq::socket_t * ptrSocket) {
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    if (e != boost::asio::error::operation_aborted) {
	 std::cout << timeLocal << ": Processing Event " << event << " from source node " << src << " to next Hop node " << dest << std::endl;

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

         std::cout << " -- LINK DOWN Event sent sent for Link " << 
                prevHopEid.nodeId << " ===> " << nextHopEid.nodeId << "" <<  std::endl;

     } else {
        std::cout << "timer dt2 cancelled\n";
    }
}

void Scheduler::ProcessLinkUp(const boost::system::error_code& e, int src, int dest, cbhe_eid_t finalDestinationEid,
	       	std::string event, zmq::socket_t * ptrSocket) {
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
	std::cout << timeLocal << ": Processing Event " << event << " from source node " << src << " to next Hop node " << dest << std::endl;

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

	std::cout << " -- LINK UP Event sent sent for Link " <<
                prevHopEid.nodeId << " ===> " << nextHopEid.nodeId << "" <<  std::endl;

    } else {
        std::cout << "timer dt cancelled\n";
    }
}

int Scheduler::ProcessContactsFile(std::string* jsonEventFileName) 
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

    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::pub);
    const std::string bind_boundSchedulerPubSubPath(
        std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));

    try {
        socket.bind(bind_boundSchedulerPubSubPath);
    } catch (const zmq::error_t & ex) {
    	std::cerr << "Scheduler socket failed to bind: " << ex.what() << std::endl;
        return false;
    }

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
                                finalDestination, "Link Available",&socket));
        vectorTimers.push_back(std::move(dt));

        dt2->expires_from_now(boost::posix_time::seconds(contactsVector[i].end + 1));                     
        dt2->async_wait(boost::bind(&Scheduler::ProcessLinkDown,this,boost::asio::placeholders::error, 
				contactsVector[i].source, 
				contactsVector[i].dest,
        			finalDestination, "Link Unavailable",&socket));
        vectorTimers2.push_back(std::move(dt2));
    }

    ioService.run();
    socket.close();

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



