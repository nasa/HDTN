/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 * Router - sends events to egress on the optimal route and next hop 
 * 
 ****************************************************************************
 */

#include "router.h"
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
#include "libcgr.h"

using namespace std;

namespace opt = boost::program_options;

const std::string Router::DEFAULT_FILE = "contactPlan_RoutingTest.json";

Router::Router() {
    m_timersFinished = false;
    m_latestTime = 0;
}

Router::~Router() {

}

void Router::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false;
}

bool Router::Run(int argc, const char* const argv[], volatile bool & running, 
		    std::string jsonEventFileName, bool useSignalHandler) {
    //Scope to ensure clean exit before return
    {
        running = true;
        m_runningFromSigHandler = true;
        m_timersFinished = false;
        jsonEventFileName = "";
        std::string contactsFile = Router::DEFAULT_FILE;

        SignalHandler sigHandler(boost::bind(&Router::MonitorExitKeypressThreadFunction, this));
        HdtnConfig_ptr hdtnConfig;
        cbhe_eid_t finalDestEid, sourceEid;	
        opt::options_description desc("Allowed options");

        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("hdtn-config-file", opt::value<std::string>()->default_value("hdtn.json"), "HDTN Configuration File.")
		("contact-plan-file", opt::value<std::string>()->default_value(Router::DEFAULT_FILE),
                "Contact Plan file needed by CGR to compute the optimal route")
                ("dest-uri-eid", opt::value<std::string>()->default_value("ipn:2.1"), "final destination Eid")
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

           std::string jsonFileName =  Router::GetFullyQualifiedFilename(contactsFile);
           if ( !boost::filesystem::exists( jsonFileName ) ) {
               std::cerr << "ContactPlan File not found: " << jsonFileName << std::endl << std::flush;
               return false;
            }
            
	    jsonEventFileName = jsonFileName;

            std::cout << "ContactPlan file: " << jsonEventFileName << std::endl;
	 

            const std::string myFinalDestUriEid = vm["dest-uri-eid"].as<string>();
            if (!Uri::ParseIpnUriString(myFinalDestUriEid, finalDestEid.nodeId, finalDestEid.serviceId)) {
                std::cerr << "error: bad dest uri string: " << myFinalDestUriEid << std::endl;
                return false;
            }

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

        std::cout << "starting Router.." << std::endl;
	
	Router router;
	const uint64_t srcNode = m_hdtnConfig.m_myNodeId; 

        //std::cout << "***srcNode****" << srcNode << std::endl;

	router.ComputeOptimalRoute(&jsonEventFileName, srcNode, finalDestEid.nodeId);

        const std::string connect_boundSchedulerPubSubPath(
        std::string("tcp://") +
        m_hdtnConfig.m_zmqSchedulerAddress +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));
        zmq::context_t ctx;
        zmq::socket_t socket(ctx, zmq::socket_type::sub);

        try {
            socket.connect(connect_boundSchedulerPubSubPath);
            socket.set(zmq::sockopt::subscribe, "");
            std::cout << "[Router] connected and listening to events from Scheduler " << connect_boundSchedulerPubSubPath << std::endl;
        }
        catch (const zmq::error_t& ex) {
            std::cerr << "error: router cannot connect to scheduler socket: " << ex.what() << std::endl;
            return false;
        }

        zmq::pollitem_t items[] = { {socket.handle(), 0, ZMQ_POLLIN, 0} };
        int rc = 0;
	
	if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "Router up and running" << std::endl;

	while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
            
            try {
                rc = zmq::poll(&items[0], 1, 250);
            }
            catch (zmq::error_t& e) {
                std::cerr << "zmq::poll threw zmq::error_t in hdtn::Router::Run: " << e.what() << std::endl;
                continue;
            }

            assert(rc >= 0);
            if (rc > 0) {
                if (items[0].revents & ZMQ_POLLIN) {

                    hdtn::IreleaseChangeHdr releaseChangeHdr;
                    const zmq::recv_buffer_result_t res = socket.recv(zmq::mutable_buffer(&releaseChangeHdr, sizeof(releaseChangeHdr)), zmq::recv_flags::none);
                    if (!res) {
                        std::cout << "[Router] unable to receive message" << endl;
                    }
                    else if ((res->truncated()) || (res->size != sizeof(releaseChangeHdr))) {
                        std::cout << "[Router] message mismatch: untruncated = " << res->untruncated_size
                            << " truncated = " << res->size << " expected = " << sizeof(releaseChangeHdr);
                    }
                    else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKDOWN) {
                        m_latestTime = releaseChangeHdr.time;
                        std::cout << "[Router] contact down: " << releaseChangeHdr.contact << std::endl;
                       // for (cbhe_eid_t& node : finalDestEids) {
                            if (m_routeTable[releaseChangeHdr.contact] == finalDestEid.nodeId) {
                                ComputeOptimalRoute(&jsonEventFileName, srcNode, finalDestEid.nodeId);
                            }
                        //}
                            std::cout << "[Router] updated time to " << m_latestTime << std::endl;
                    }
                    else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKUP) {
                        m_latestTime = releaseChangeHdr.time;
                        std::cout << "[Router] contact up" << std::endl;
                        std::cout << "[Router] updated time to " << m_latestTime << std::endl;
                    }
                    else {
                    	std::cerr << "[Router] unknown message type " << releaseChangeHdr.base.type << std::endl;
                    }

                    
                }
            }

	}

        m_timersFinished = true;

        boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();

        std::cout << "Router currentTime  " << timeLocal << std::endl << std::flush;
    }
    std::cout << "Router exiting cleanly..\n";
    return true;
}

void Router::RouteUpdate(const boost::system::error_code& e, uint64_t nextHopNodeId,
    uint64_t finalDestNodeId, std::string event, zmq::socket_t * ptrSocket)
{

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << timeLocal << ": [Router] Sending RouteUpdate event to Egress " << std::endl;

        hdtn::RouteUpdateHdr routingMsg;
        memset(&routingMsg, 0, sizeof(hdtn::RouteUpdateHdr));
        routingMsg.base.type = HDTN_MSGTYPE_ROUTEUPDATE;
        routingMsg.nextHopNodeId = nextHopNodeId;
        routingMsg.finalDestNodeId = finalDestNodeId;
        ptrSocket->send(zmq::const_buffer(&routingMsg, sizeof(hdtn::RouteUpdateHdr)),
                        zmq::send_flags::none);

    }
    else {
        std::cout << "timer dt cancelled\n";
    }
}

int Router::ComputeOptimalRoute(std::string* jsonEventFileName, uint64_t sourceNode, uint64_t finalDestNodeId)
{
    m_timersFinished = false;

    std::cout << "[Router] Reading contact plan and computing next hop" << std::endl;
    std::vector<cgr::Contact> contactPlan = cgr::cp_load(*jsonEventFileName);

    cgr::Contact rootContact = cgr::Contact(sourceNode, sourceNode, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
    rootContact.arrival_time = m_latestTime;
    cgr::Route bestRoute = cgr::dijkstra(&rootContact, finalDestNodeId, contactPlan);
    //cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, finalDestEid.nodeId, contactPlan);

    const uint64_t nextHop = bestRoute.next_node;

    m_routeTable[bestRoute.get_hops()[0].id + 1] = finalDestNodeId;
    
    std::cout << "[Router] Computed next hop: " << nextHop << std::endl;
    
    //if (bestRoute != NULL) { // successfully computed a route
        const uint64_t nextHopNodeId = bestRoute.next_node;

        std::cout << "[Router] CGR computed next hop: " << nextHopNodeId << std::endl;

        cbhe_eid_t nextHopEid;
        nextHopEid.nodeId = nextHop;
        nextHopEid.serviceId= 1;


        //boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
        //std::cout << "[Router] Local Time:  " << timeLocal << std::endl << std::flush;

        zmq::context_t ctx;
        zmq::socket_t socket(ctx, zmq::socket_type::pub);

        const std::string bind_boundRouterPubSubPath(
            std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundRouterPubSubPortPath));
        try {
            socket.bind(bind_boundRouterPubSubPath);
            std::cout << "[Router] socket bound successfully to  " << bind_boundRouterPubSubPath << std::endl;
        }
        catch (const zmq::error_t& ex) {
            std::cerr << "[Router] socket failed to bind: " << ex.what() << std::endl;
            return 0;
        }

        boost::asio::io_service ioService;

        SmartDeadlineTimer dt = boost::make_unique<boost::asio::deadline_timer>(ioService);

        dt->expires_from_now(boost::posix_time::seconds(1));
        dt->async_wait(boost::bind(&Router::RouteUpdate, this, boost::asio::placeholders::error,
            nextHopNodeId, finalDestNodeId, "RouteUpdate", &socket));
        ioService.run();

        socket.close();

        m_timersFinished = true;
    //}
    //else {
        // what should we do if no route is found?
    //}
    
    return 0;
}
