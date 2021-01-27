#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include "paths.hpp"
#include "reg.hpp"

int main(int argc, char *argv[]) {
  hdtn::HdtnRegsvr reg;
  reg.Init(HDTN_REG_SERVER_PATH, "egress", 10120, "pull");
  reg.Reg();
  zmq::context_t ctx;
  zmq::socket_t socket(ctx, zmq::socket_type::pull);
  socket.bind(HDTN_RELEASE_PATH);

  zmq::message_t message;
  while (true) {
    socket.recv(&message);
    message.data();
  }

  return 0;
}
