#include "cgrServer.h"

void CgrServer::init(std::string address)
{
    std::cout << "starting init" << std::endl;
    std::cout << "socket reset" << std::endl;
    cgrSock.reset(); //delete existing
    std::cout << "create context" << std::endl;

    cgrCtx = boost::make_unique<zmq::context_t>();
    std::cout << "create socket" << std::endl;

    cgrSock = boost::make_unique<zmq::socket_t>(*cgrCtx, zmq::socket_type::pair);
    char tbuf[255];
//    memset(tbuf, 0, 255);

    std::cout << "set socket" << std::endl;
  //  cgrSock->set(zmq::sockopt::routing_id, tbuf);
    static const int timeout = 2000;
    cgrSock->set(zmq::sockopt::rcvtimeo, timeout);
    std::cout << "attempting zmq connection" << std::endl;
    cgrSock->connect(address); //"tcp://localhost:5556"
}

int CgrServer::requestNextHop(int currentNode, int destinationNode, int startTime)
{
    std::cout << "creating message" << std::endl;

    std::string message = std::to_string(currentNode) + "|" + std::to_string(destinationNode) 
        + "|" + std::to_string(startTime);


    //cgrSock->send(&msg, zmq::send_flags::none);
    std::cout << "Sending CGR request" << std::endl;
    cgrSock->send(zmq::const_buffer(message.c_str(), strlen(message.c_str())), zmq::send_flags::none);
    std::cout << "set to receive message back" << std::endl;
    zmq::message_t recvMsg;
    cgrSock->recv(recvMsg, zmq::recv_flags::none);

    std::cout << std::to_string((uintptr_t)recvMsg.data()) << std::endl;
    uintptr_t nextNode = (uintptr_t)recvMsg.data();
    std::cout << "Next hop is " << std::to_string(nextNode) << std::endl;
    return nextNode;
}

