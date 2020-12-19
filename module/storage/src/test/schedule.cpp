#include <fcntl.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdlib>
#include <iostream>

#include "message.hpp"
#include "reg.hpp"
#include "paths.hpp"

//This test code is used to send storage release messages 
//to enable development of the contact schedule and bundle
//storage release mechanism.
//release.cpp implements a subscriber for these messages. 
int main(int argc, char *argv[]) {
    hdtn::hdtn_regsvr _reg;
    _reg.init(HDTN_REG_SERVER_PATH, "scheduler", 10200, "pub");
    _reg.reg();
    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::pub);
    socket.bind(HDTN_SCHEDULER_PATH);
    boost::asio::io_service io;
    char relHdr[sizeof(hdtn::irelease_start_hdr)];
    hdtn::irelease_start_hdr *releaseMsg = (hdtn::irelease_start_hdr *)relHdr;
    char stopHdr[sizeof(hdtn::irelease_stop_hdr)];
    hdtn::irelease_stop_hdr *stopMsg = (hdtn::irelease_stop_hdr *)stopHdr;

    for (uint32_t i = 0; i < 100; i++) {
        memset(relHdr, 0, sizeof(hdtn::irelease_start_hdr));
        releaseMsg->base.type = HDTN_MSGTYPE_IRELSTART;
        releaseMsg->flow_id = i;
        releaseMsg->rate = 0;  //go as fast as possible
        releaseMsg->duration = 20;
        socket.send(releaseMsg, sizeof(hdtn::irelease_start_hdr), 0);
        std::cout << "release for 20 seconds \n";
        boost::asio::deadline_timer timer(io, boost::posix_time::seconds(20));
        timer.wait();
        memset(stopHdr, 0, sizeof(hdtn::irelease_stop_hdr));
        stopMsg->base.type = HDTN_MSGTYPE_IRELSTOP;
        stopMsg->flow_id = i;
        socket.send(stopMsg, sizeof(hdtn::irelease_stop_hdr), 0);
        std::cout << "stop release \n";
    }

    return 0;
}
