#include "cgrServer.h"

void CgrServer::init(std::string address)
{
    cgrSock.reset(); //delete existing
    cgrCtx = boost::make_unique<zmq::context_t>();
    cgrSock = boost::make_unique<zmq::socket_t>(*cgrCtx, zmq::socket_type::req);
    char tbuf[255];
    memset(tbuf, 0, 255);
    cgrSock->set(zmq::sockopt::routing_id, tbuf);
    static const int timeout = 2000;
    cgrSock->set(zmq::sockopt::rcvtimeo, timeout);
    cgrSock->connect(address); //"tcp://localhost:5556"
}

int CgrServer::requestNextHop(int currentNode, int destinationNode, int startTime)
{
    Message msg;
    msg.current = currentNode;
    msg.destination = destinationNode;
    msg.start = startTime;

    std::string message = std::to_string(currentNode) + "|" + std::to_string(destinationNode) 
        + "|" + std::to_string(startTime);


    //cgrSock->send(&msg, zmq::send_flags::none);
    std::cout << "Sending CGR request" << std::endl;
    cgrSock->send(zmq::const_buffer(message.c_str(), strlen(message.c_str())), zmq::send_flags::none);
    zmq::message_t recvMsg;
    cgrSock->recv(recvMsg, zmq::recv_flags::none);
    uintptr_t nextNode = (uintptr_t)recvMsg.data();
    std::cout << "Next hop is " << std::to_string(nextNode) << std::endl;
    return reinterpret_cast<size_t>(nextNode);
}

