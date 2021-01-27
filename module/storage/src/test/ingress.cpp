#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include "message.hpp"
#include "paths.hpp"
#include "reg.hpp"

int main(int argc, char *argv[]) {
  hdtn::HdtnRegsvr reg;
  reg.Init(HDTN_REG_SERVER_PATH, "ingress", 10110, "push");
  reg.Reg();
  zmq::context_t ctx;
  zmq::socket_t socket(ctx, zmq::socket_type::push);
  socket.bind(HDTN_STORAGE_PATH);

  int rfd = open("/dev/urandom", O_RDONLY);
  if (rfd < 0) {
    perror("Failed to open /dev/urandom: ");
    return -1;
  }

  char data[8192];
  read(rfd, data, 8192);

  while (true) {
    char ihdr[sizeof(hdtn::BlockHdr)];
    hdtn::BlockHdr *block = (hdtn::BlockHdr *)ihdr;
    memset(ihdr, 0, sizeof(hdtn::BlockHdr));
    block->base.type = HDTN_MSGTYPE_STORE;
    block->flow_id = rand() % 65536;
    socket.send(ihdr, sizeof(hdtn::BlockHdr), ZMQ_MORE);
    socket.send(data, 1024, 0);
  }

  return 0;
}
