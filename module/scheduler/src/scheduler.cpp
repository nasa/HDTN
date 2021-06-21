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

const std::string Scheduler::DEFAULT_FILE = "contactPlan.json";

Scheduler::Scheduler() {
    m_timersFinished = false;
}

Scheduler::~Scheduler() {

}

void Scheduler::ProcessLinkDown(const boost::system::error_code& e, int id, std::string event, zmq::socket_t * ptrSocket) {
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    m_dt2TimerIsRunning = false;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
         std::cout <<  "Processing Event at Expiry time: " << timeLocal << " , for flow id: " << id << " , Event is " << event;
         hdtn::IreleaseStopHdr stopMsg;
         memset(&stopMsg, 0, sizeof(hdtn::IreleaseStopHdr));
         stopMsg.base.type = HDTN_MSGTYPE_ILINKDOWN;
         stopMsg.flowId = id;
         ptrSocket->send(zmq::const_buffer(&stopMsg, sizeof(hdtn::IreleaseStopHdr)), zmq::send_flags::none);
         std::cout << " -- Stop Release message sent. for flow id" << stopMsg.flowId << std::endl;
     } else {
        std::cout << "timer dt2 cancelled\n";
    }
}

void Scheduler::ProcessLinkUp(const boost::system::error_code& e, int id, std::string event, zmq::socket_t * ptrSocket) {
    m_dtTimerIsRunning = false;
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();

    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
    std::cout <<  "Processing Event at Expiry time: " << timeLocal << " , for flow id: " << id << " , Event is " << event;
    hdtn::IreleaseStartHdr releaseMsg;
    memset(&releaseMsg, 0, sizeof(hdtn::IreleaseStartHdr));
    releaseMsg.base.type = HDTN_MSGTYPE_ILINKUP;
    releaseMsg.flowId = id;
    ptrSocket->send(zmq::const_buffer(&releaseMsg, sizeof(hdtn::IreleaseStartHdr)), zmq::send_flags::none);
    std::cout << " -- Start Release message sent. for flow id" << releaseMsg.flowId << std::endl;
    } else {
        std::cout << "timer dt cancelled\n";
    }
}

int Scheduler::ProcessContactsFile(std::string jsonEventFileName) {
    m_timersFinished = false;
    contactPlanVector_t contactsVector;
    boost::property_tree::ptree pt = JsonSerializable::GetPropertyTreeFromJsonFile(jsonEventFileName);
    const boost::property_tree::ptree & contactsPt
            = pt.get_child("contacts", boost::property_tree::ptree());
    contactsVector.resize(contactsPt.size());
    unsigned int eventIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, contactsPt) {
	contactPlan_t & linkEvent = contactsVector[eventIndex++];
        linkEvent.contact = eventPt.second.get<int>("contact",0);
	linkEvent.source = eventPt.second.get<int>("source",0);
	linkEvent.dest = eventPt.second.get<int>("dest",0);
	linkEvent.id = eventPt.second.get<int>("flow id",0);
	linkEvent.start = eventPt.second.get<int>("start time",0);
        linkEvent.end = eventPt.second.get<int>("end time",0);
	linkEvent.rate = eventPt.second.get<int>("rate",0);

        std::string errorMessage = "";
        if (linkEvent.id < 0 ) {
            errorMessage += " Invalid id: " + boost::lexical_cast<std::string>(linkEvent.id) + ".";
        }

	if (errorMessage.length() > 0) {
            std::cerr << errorMessage << std::endl << std::flush;
            return 1;
        }
    }

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    boost::posix_time::ptime epochTime = boost::posix_time::second_clock::local_time();
    std::cout << "Epoch Time:  " << epochTime << std::endl << std::flush;
    std::cout << " Time local:  " << timeLocal << std::endl << std::flush;

    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::pub);
    const std::string bind_boundSchedulerPubSubPath(
        std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));
    socket.bind(bind_boundSchedulerPubSubPath);

    boost::asio::io_service ioService;

    std::vector<SmartDeadlineTimer> vectorTimers;
    vectorTimers.reserve(contactsVector.size());

    std::vector<SmartDeadlineTimer> vectorTimers2;
    vectorTimers2.reserve(contactsVector.size());


    for(std::size_t i=0; i < contactsVector.size(); ++i) {
        SmartDeadlineTimer dt = boost::make_unique<boost::asio::deadline_timer>(ioService);
        SmartDeadlineTimer dt2 = boost::make_unique<boost::asio::deadline_timer>(ioService);
        std::cout << "Expiration time for available links: flowid " << contactsVector[i].id << "is " << epochTime + boost::posix_time::seconds(contactsVector[i].start) << std::endl << std::flush;
        std::cout << "Expiration time for unavailable links: flowid " << contactsVector[i].id << "is " << epochTime + boost::posix_time::seconds(contactsVector[i].end+1) << std::endl << std::flush;

        dt->expires_from_now(boost::posix_time::seconds(contactsVector[i].start));
        dt->async_wait(boost::bind(&Scheduler::ProcessLinkUp,this,boost::asio::placeholders::error, contactsVector[i].id,
                       "Link available",&socket));
        m_dtTimerIsRunning = true;
        vectorTimers.push_back(std::move(dt));

        dt2->expires_from_now(boost::posix_time::seconds(contactsVector[i].end + 1));                     
        dt2->async_wait(boost::bind(&Scheduler::ProcessLinkDown,this,boost::asio::placeholders::error, contactsVector[i].id,
                                   "Link unavailable",&socket));
        m_dt2TimerIsRunning = true;
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



