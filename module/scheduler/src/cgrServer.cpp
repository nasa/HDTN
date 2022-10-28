#include "cgrServer.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::scheduler;

void CgrServer::init(std::string address)
{
    LOG_INFO(subprocess) << "starting init";
    LOG_INFO(subprocess) << "socket reset";
    cgrSock.reset(); //delete existing
    LOG_INFO(subprocess) << "create context";

    cgrCtx = boost::make_unique<zmq::context_t>();
    LOG_INFO(subprocess) << "create socket";

    cgrSock = boost::make_unique<zmq::socket_t>(*cgrCtx, zmq::socket_type::pair);

    LOG_INFO(subprocess) << "set socket";
    static const int timeout = 2000;
    cgrSock->set(zmq::sockopt::rcvtimeo, timeout);
    LOG_INFO(subprocess) << "attempting zmq connection";
    cgrSock->connect(address); //"tcp://localhost:5556"
}

int CgrServer::requestNextHop(int currentNode, int destinationNode, int startTime)
{
    std::string message = std::to_string(currentNode) + "|" + std::to_string(destinationNode) 
        + "|" + std::to_string(startTime);

    //cgrSock->send(&msg, zmq::send_flags::none);
    LOG_INFO(subprocess) << "Sending CGR request";
    cgrSock->send(zmq::const_buffer(message.c_str(), strlen(message.c_str())), zmq::send_flags::none);
    LOG_INFO(subprocess) << "Waiting to receive message back";
    zmq::message_t recvMessage;
    cgrSock->recv(recvMessage, zmq::recv_flags::none);

    std::string myReceivedAsString((const char *)recvMessage.data(), (const char *)recvMessage.data() + recvMessage.size());

    //uintptr_t nextNode = (uintptr_t)recvMsg.data();
    LOG_INFO(subprocess) << "Next hop is " << myReceivedAsString;
    return stoi(myReceivedAsString); //nextNode;
}

