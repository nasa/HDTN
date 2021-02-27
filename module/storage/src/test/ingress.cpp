#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>

#include "message.hpp"
#include "paths.hpp"
#include "reg.hpp"

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    hdtn::HdtnRegsvr reg;
    reg.Init(HDTN_REG_SERVER_PATH, "ingress", 10110, "push");
    reg.Reg();
    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::push);
    socket.bind(HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH); //TODO: this only supports cut-through

    int rfd = open("/dev/urandom", O_RDONLY);
    if (rfd < 0) {
        perror("Failed to open /dev/urandom: ");
        return -1;
    }

    char data[8192];
    ssize_t bytesRead = read(rfd, data, 8192);
    (void) bytesRead;

    while (true) {
        hdtn::BlockHdr block;
        memset(&block, 0, sizeof(hdtn::BlockHdr));
        block.base.type = HDTN_MSGTYPE_STORE;
        block.flowId = rand() % 65536;
        socket.send(zmq::const_buffer(&block, sizeof(hdtn::BlockHdr)), zmq::send_flags::sndmore); //TODO: ZMQ_MORE probably wrong
        socket.send(zmq::const_buffer(data, 1024), zmq::send_flags::none);
    }

    return 0;
}
