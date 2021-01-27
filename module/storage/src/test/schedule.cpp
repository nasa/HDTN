#include <fcntl.h>
#include <unistd.h>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdlib>
#include <iostream>
#include "message.hpp"
#include "paths.hpp"
#include "reg.hpp"

// This test code is used to send storage release messages
// to enable development of the contact schedule and bundle
// storage release mechanism.
// release.cpp implements a subscriber for these messages.

int main(int argc, char *argv[]) {
  hdtn::HdtnRegsvr reg;
  reg.Init(HDTN_REG_SERVER_PATH, "scheduler", 10200, "pub");
  reg.Reg();
  zmq::context_t ctx;
  zmq::socket_t socket(ctx, zmq::socket_type::pub);
  socket.bind(HDTN_SCHEDULER_PATH);
  boost::asio::io_service io;
  char rel_hdr[sizeof(hdtn::IreleaseStartHdr)];
  hdtn::IreleaseStartHdr *release_msg = (hdtn::IreleaseStartHdr *)rel_hdr;
  char stop_hdr[sizeof(hdtn::IreleaseStopHdr)];
  hdtn::IreleaseStopHdr *stop_msg = (hdtn::IreleaseStopHdr *)stop_hdr;

  for (uint32_t i = 0; i < 100; i++) {
    memset(rel_hdr, 0, sizeof(hdtn::IreleaseStartHdr));
    release_msg->base.type = HDTN_MSGTYPE_IRELSTART;
    release_msg->flow_id = i;
    release_msg->rate = 0;  // go as fast as possible
    release_msg->duration = 20;
    socket.send(release_msg, sizeof(hdtn::IreleaseStartHdr), 0);
    std::cout << "release for 20 seconds \n";
    boost::asio::deadline_timer timer(io, boost::posix_time::seconds(20));
    timer.wait();
    memset(stop_hdr, 0, sizeof(hdtn::IreleaseStopHdr));
    stop_msg->base.type = HDTN_MSGTYPE_IRELSTOP;
    stop_msg->flow_id = i;
    socket.send(stop_msg, sizeof(hdtn::IreleaseStopHdr), 0);
    std::cout << "stop release \n";
  }

  return 0;
}
