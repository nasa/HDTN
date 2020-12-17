#include <fcntl.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdlib>
#include <iostream>

#include "message.hpp"
#include "reg.hpp"

int main(int argc, char *argv[]) {
    hdtn::hdtn_regsvr _reg;
    _reg.init("tcp://localhost:10140", "scheduler", 10149, "pub");
    _reg.reg();
    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::pub);
    socket.bind("tcp://127.0.0.1:10149");
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
        releaseMsg->duration = 60;
        socket.send(releaseMsg, sizeof(hdtn::irelease_start_hdr), 0);
        std::cout << "release for 60 seconds \n";
        boost::asio::deadline_timer timer(io, boost::posix_time::seconds(60));
        timer.wait();
        memset(stopHdr, 0, sizeof(hdtn::irelease_stop_hdr));
        stopMsg->base.type = HDTN_MSGTYPE_IRELSTOP;
        stopMsg->flow_id = i;
        socket.send(stopMsg, sizeof(hdtn::irelease_stop_hdr), 0);
        std::cout << "stop release \n";
    }

    return 0;
}
