/**
 * @file ReleaseSender.cpp
 * @author  Jeff Follo
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "ReleaseSender.h"
#include "Uri.h"

const std::string ReleaseSender::DEFAULT_FILE = "releaseMessages1.json";


ReleaseSender::ReleaseSender() {
    m_timersFinished = false;
}

ReleaseSender::~ReleaseSender() {

}

void ReleaseSender::ProcessEvent(const boost::system::error_code&, const cbhe_eid_t finalDestinationEid, std::string message, zmq::socket_t * ptrSocket) {
  boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
  LOG_INFO(hdtn::Logger::Module::storage) <<  "Expiry time: " << timeLocal << " , finalDestinationEid: (" << finalDestinationEid.nodeId << "," << finalDestinationEid.serviceId << ") , message: " << message;
  if (message == "start") {
      hdtn::IreleaseStartHdr releaseMsg;
      memset(&releaseMsg, 0, sizeof(hdtn::IreleaseStartHdr));
      releaseMsg.base.type = HDTN_MSGTYPE_ILINKUP;
      releaseMsg.finalDestinationNodeId = finalDestinationEid.nodeId;
      releaseMsg.rate = 0;  //not implemented
      releaseMsg.duration = 20;  //not implemented
      ptrSocket->send(zmq::const_buffer(&releaseMsg, sizeof(hdtn::IreleaseStartHdr)), zmq::send_flags::none);
      LOG_INFO(hdtn::Logger::Module::storage) << " -- Start Release message sent.";
  }
  else if (message == "stop") {
      hdtn::IreleaseStopHdr stopMsg;
      memset(&stopMsg, 0, sizeof(hdtn::IreleaseStopHdr));
      stopMsg.base.type = HDTN_MSGTYPE_ILINKDOWN;
      stopMsg.finalDestinationNodeId = finalDestinationEid.nodeId;
      ptrSocket->send(zmq::const_buffer(&stopMsg, sizeof(hdtn::IreleaseStopHdr)), zmq::send_flags::none);
      LOG_INFO(hdtn::Logger::Module::storage) << " -- Stop Release message sent.";
  }
}

int ReleaseSender::ProcessEventFile(std::string jsonEventFileName) {
    m_timersFinished = false;
    ReleaseMessageEventVector_t releaseMessageEventVector;
    boost::property_tree::ptree pt = JsonSerializable::GetPropertyTreeFromJsonFile(jsonEventFileName);
    const boost::property_tree::ptree & releaseMessageEventsPt
            = pt.get_child("releaseMessageEvents", boost::property_tree::ptree());
    releaseMessageEventVector.resize(releaseMessageEventsPt.size());
    unsigned int eventIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, releaseMessageEventsPt) {
        ReleaseMessageEvent_t & releaseMessageEvent = releaseMessageEventVector[eventIndex++];
        releaseMessageEvent.message = eventPt.second.get<std::string>("message", "");
        const std::string uriEid = eventPt.second.get<std::string>("finalDestinationEid", "");
        if (!Uri::ParseIpnUriString(uriEid, releaseMessageEvent.finalDestEid.nodeId, releaseMessageEvent.finalDestEid.serviceId)) {
            LOG_ERROR(hdtn::Logger::Module::storage) << "error: bad uri string: " << uriEid;
            return false;
        }
        releaseMessageEvent.delay = eventPt.second.get<int>("delay",0);
        std::string errorMessage = "";
        if ( (releaseMessageEvent.message != "start") && (releaseMessageEvent.message != "stop") ) {
            errorMessage += " Invalid message: " + releaseMessageEvent.message + ".";
        }
        if ( releaseMessageEvent.delay < 0 ) {
            errorMessage += " Invalid delay: " + boost::lexical_cast<std::string>(releaseMessageEvent.delay) + ".";
        }
        if (errorMessage.length() > 0) {
            LOG_ERROR(hdtn::Logger::Module::storage) << errorMessage;
            return 1;
        }
    }

    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(hdtn::Logger::Module::storage) << "Epoch Time:  " << timeLocal;

    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::pub);
    const std::string bind_boundSchedulerPubSubPath(
        std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));
    socket.bind(bind_boundSchedulerPubSubPath);

    boost::asio::io_service ioService;
    std::vector<SmartDeadlineTimer> vectorTimers;
    vectorTimers.reserve(releaseMessageEventVector.size());
    for(std::size_t i=0; i<releaseMessageEventVector.size(); ++i) {
        SmartDeadlineTimer dt = boost::make_unique<boost::asio::deadline_timer>(ioService);
        dt->expires_from_now(boost::posix_time::seconds(releaseMessageEventVector[i].delay));
        dt->async_wait(boost::bind(&ReleaseSender::ProcessEvent,this,boost::asio::placeholders::error, releaseMessageEventVector[i].finalDestEid,
                                   releaseMessageEventVector[i].message,&socket));
        vectorTimers.push_back(std::move(dt));
    }
    ioService.run();

    socket.close();
    m_timersFinished = true;
    timeLocal = boost::posix_time::second_clock::local_time();
    LOG_INFO(hdtn::Logger::Module::storage) << "End of ProcessEventFile:  " << timeLocal << std::endl << std::flush;
    return 0;
}

int ReleaseSender::ProcessComandLine(int argc, const char *argv[], std::string& jsonEventFileName) {
    jsonEventFileName = "";
    std::string eventsFile = ReleaseSender::DEFAULT_FILE;
    boost::program_options::options_description desc("Allowed options");
    try {
        desc.add_options()
            ("help", "Produce help message.")
            ("hdtn-config-file", boost::program_options::value<std::string>()->default_value("hdtn.json"), "HDTN Configuration File.")
            ("events-file", boost::program_options::value<std::string>()->default_value(ReleaseSender::DEFAULT_FILE),
             "Name of events file.");
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc,
                boost::program_options::command_line_style::unix_style
               | boost::program_options::command_line_style::case_insensitive), vm);
        boost::program_options::notify(vm);
        if (vm.count("help")) {
            LOG_INFO(hdtn::Logger::Module::storage) << desc;
            return 1;
        }
        eventsFile = vm["events-file"].as<std::string>();
        if (eventsFile.length() < 1) {
            LOG_INFO(hdtn::Logger::Module::storage) << desc;
            return 1;
        }
        const std::string configFileName = vm["hdtn-config-file"].as<std::string>();

        if(HdtnConfig_ptr ptrConfig = HdtnConfig::CreateFromJsonFile(configFileName)) {
            m_hdtnConfig = *ptrConfig;
        }
        else {
            LOG_ERROR(hdtn::Logger::Module::storage) << "error loading config file: " << configFileName;
            return false;
        }
    }
    catch (std::exception& e) {
        LOG_ERROR(hdtn::Logger::Module::storage) << "error: " << e.what();
        return 1;
    }
    catch (...) {
        LOG_ERROR(hdtn::Logger::Module::storage) << "Exception of unknown type!";
        return 1;
    }
    std::string jsonFileName =  ReleaseSender::GetFullyQualifiedFilename(eventsFile);
    if ( !boost::filesystem::exists( jsonFileName ) ) {
        LOG_ERROR(hdtn::Logger::Module::storage) << "File not found: " << jsonFileName;
        return 1;
    }
    jsonEventFileName = jsonFileName;
    return 0;
}



