#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <iostream>
#include "reg.hpp"

int main(int argc, char* argv[]) {
    hdtn3::hdtn3_regsvr _reg;
    _reg.init("tcp://localhost:10140", "egress", 10148, "push");
    _reg.reg();
    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::push);
    socket.bind("tcp://localhost:10148");

    zmq::message_t message;
    while(true) {
        socket.recv(&message);
        message.data();
    }

    return 0;
}
