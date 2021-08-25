#include <boost/asio.hpp>
#include <cstdint>
#include "HdtnConfig.h"
#include <boost/make_unique.hpp>
#include <zmq.hpp>

class CgrServer
{
    struct Message
    {
        uint16_t current; //current node
        uint16_t destination; //destination node
        uint16_t start; //start time

    };

    std::unique_ptr<zmq::context_t> cgrCtx;
    std::unique_ptr<zmq::socket_t> cgrSock;

    void init(std::string address);
    int requestNextHop(int currentNode, int destinationNode, int startTime);
    

};
