#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>

#include "reg.hpp"
#include "paths.hpp"

int main(int argc, char *argv[]) {
    hdtn::hdtn_regsvr _reg;
    _reg.init(HDTN_REG_SERVER_PATH, "egress", 10120, "pull");
    _reg.reg();
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
