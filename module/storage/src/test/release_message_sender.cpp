/**
 * @file release_message_sender.cpp
 * @author  Jeff Follo
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

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <iostream>
#include "HdtnConfig.h"

#include "message.hpp"
#include "Uri.h"
#include "zmq.hpp"

//This test code is used to send storage release messages
//to enable development of the contact schedule and bundle
//storage release mechanism.


int main(int argc, char *argv[]) {

    bool isStartMessage = false;
    cbhe_eid_t finalDestEidToRelease;
    uint64_t nextHopNodeNumber;
    unsigned int delayBeforeSendSeconds = 0;
    HdtnConfig_ptr hdtnConfig;

    boost::program_options::options_description desc("Allowed options");
    try {
        desc.add_options()
            ("help", "Produce help message.")
            ("release-message-type", boost::program_options::value<std::string>()->default_value("start"), "Send a start or stop message.")
            ("dest-uri-eid-to-release-or-stop", boost::program_options::value<std::string>(), "IPN uri final destination to release or stop.")
            ("dest-node-number-to-release-or-stop", boost::program_options::value<uint64_t>(), "Final destination node number to release or stop.")
            ("next-hop-node-number", boost::program_options::value<uint64_t>(), "Final destination node number to release or stop.")
            ("delay-before-send", boost::program_options::value<unsigned int>()->default_value(0), "Seconds before send.")
            ("hdtn-config-file", boost::program_options::value<std::string>()->default_value("hdtn.json"), "HDTN Configuration File.")
            ;

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        const std::string configFileName = vm["hdtn-config-file"].as<std::string>();

        hdtnConfig = HdtnConfig::CreateFromJsonFile(configFileName);
        if (!hdtnConfig) {
            std::cerr << "error loading config file: " << configFileName << std::endl;
            return false;
        }

        const std::string msgType = vm["release-message-type"].as<std::string>();
        if (boost::iequals(msgType, "start")) {
            isStartMessage = true;
        }
        else if (boost::iequals(msgType, "stop")) {
            isStartMessage = false;
        }
        else {
            std::cout << desc << "\n";
            return 1;
        }

        const bool hasEid = (vm.count("dest-uri-eid-to-release-or-stop") != 0);
        const bool hasNodeNumber = (vm.count("dest-node-number-to-release-or-stop") != 0);
        if (hasEid && hasNodeNumber) {
            std::cerr << "error: cannot have both dest-uri-eid-to-release-or-stop and dest-node-number-to-release-or-stop specified\n";
            return false;
        }
        if (!(hasEid || hasNodeNumber)) {
            std::cerr << "error: must have one of dest-uri-eid-to-release-or-stop and dest-node-number-to-release-or-stop specified\n";
            return false;
        }

        if (hasEid) {
            std::cout << "deprecation warning: dest-uri-eid-to-release-or-stop should be replaced with dest-node-number-to-release-or-stop\n";
            const std::string uriEid = vm["dest-uri-eid-to-release-or-stop"].as<std::string>();
            if (!Uri::ParseIpnUriString(uriEid, finalDestEidToRelease.nodeId, finalDestEidToRelease.serviceId)) {
                std::cerr << "error: bad uri string: " << uriEid << std::endl;
                return false;
            }
        }
        else {
            finalDestEidToRelease.nodeId = vm["dest-node-number-to-release-or-stop"].as<uint64_t>();
            finalDestEidToRelease.serviceId = 0; //dont care (unused)
        }

        if (vm.count("next-hop-node-number")) {
            nextHopNodeNumber = vm["next-hop-node-number"].as<uint64_t>();
        }
        else {
            std::cout << "warning: next-hop-node-number was not specified, assuming final destination node number is the next hop\n";
            nextHopNodeNumber = finalDestEidToRelease.nodeId;
        }


        delayBeforeSendSeconds = vm["delay-before-send"].as<unsigned int>();
    }
    catch (boost::bad_any_cast & e) {
        std::cout << "invalid data error: " << e.what() << "\n\n";
        std::cout << desc << "\n";
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch (...) {
        std::cerr << "Exception of unknown type!\n";
        return 1;
    }
   
    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::pub);
    const std::string bind_boundSchedulerPubSubPath(
        std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnConfig->m_zmqBoundSchedulerPubSubPortPath));
    socket.bind(bind_boundSchedulerPubSubPath);

    std::cout << "waiting " << delayBeforeSendSeconds << " seconds..." << std::endl;
    boost::this_thread::sleep(boost::posix_time::seconds(delayBeforeSendSeconds));

    if (isStartMessage) {
        hdtn::IreleaseStartHdr releaseMsg;
        memset(&releaseMsg, 0, sizeof(hdtn::IreleaseStartHdr));
        releaseMsg.base.type = HDTN_MSGTYPE_ILINKUP;
        releaseMsg.finalDestinationNodeId = finalDestEidToRelease.nodeId; //todo
        releaseMsg.nextHopNodeId = nextHopNodeNumber;
        releaseMsg.rate = 0;  //not implemented
        releaseMsg.duration = 20;//not implemented
        socket.send(zmq::const_buffer(&releaseMsg, sizeof(hdtn::IreleaseStartHdr)), zmq::send_flags::none);
        std::cout << "Start Release message sent \n";
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
    else {
        hdtn::IreleaseStopHdr stopMsg;
        memset(&stopMsg, 0, sizeof(hdtn::IreleaseStopHdr));
        stopMsg.base.type = HDTN_MSGTYPE_ILINKDOWN;
        stopMsg.finalDestinationNodeId = finalDestEidToRelease.nodeId;
        stopMsg.nextHopNodeId = nextHopNodeNumber;
        socket.send(zmq::const_buffer(&stopMsg, sizeof(hdtn::IreleaseStopHdr)), zmq::send_flags::none);
        std::cout << "Stop Release message sent \n";
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
 
   

    return 0;
}
