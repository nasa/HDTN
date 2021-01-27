#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include "message.hpp"
#include "paths.hpp"
#include "reg.hpp"
#include "zmq.hpp"

// This test code is used to receive storage release messages
// to enable development of the contact schedule and bundle
// storage release mechanism.
// schedule.cpp implements the publisher for these messages.

int main(int argc, char *argv[]) {
  // Registration server commented out, multiple subscribers will hang otherwise
  // Will investigate time permitting
  // hdtn::hdtn_regsvr _reg;
  //_reg.init(HDTN_REG_SERVER_PATH, "release", 10200, "sub");
  //_reg.reg();
  zmq::context_t ctx;
  zmq::socket_t socket(ctx, zmq::socket_type::sub);
  socket.connect(HDTN_SCHEDULER_PATH);
  socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);

  zmq::message_t message;
  while (true) {
    socket.recv(&message);
    std::cout << "message received\n";
    hdtn::CommonHdr *common = (hdtn::CommonHdr *)message.data();
    switch (common->type) {
      case HDTN_MSGTYPE_IRELSTART:
        std::cout << "release data\n";
        break;
      case HDTN_MSGTYPE_IRELSTOP:
        std::cout << "stop releasing data\n";
        break;
    }
  }

  return 0;
}
