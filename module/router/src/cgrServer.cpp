#include "cgrServer.h"
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

void CgrServer::init(const std::string & address)
{
    std::cout << "starting init" << std::endl;
    std::cout << "socket reset" << std::endl;
    cgrSock.reset(); //delete existing
    std::cout << "create context" << std::endl;

    cgrCtx = boost::make_unique<zmq::context_t>();
    std::cout << "create socket" << std::endl;

    cgrSock = boost::make_unique<zmq::socket_t>(*cgrCtx, zmq::socket_type::pair);

    std::cout << "set socket" << std::endl;
    static const int timeout = 2000;
    cgrSock->set(zmq::sockopt::rcvtimeo, timeout);
    std::cout << "attempting zmq connection" << std::endl;
    cgrSock->connect(address); //"tcp://localhost:5556"
}

uint64_t CgrServer::requestNextHop(uint64_t currentNode, uint64_t destinationNode, uint64_t startTime)
{
    //const std::string message = boost::lexical_cast<std::string>(currentNode) + "|" + boost::lexical_cast<std::string>(destinationNode)
    //    + "|" + boost::lexical_cast<std::string>(startTime);
    static const boost::format fmtTemplate("%d|%d|%d");
    boost::format fmt(fmtTemplate);
    fmt % currentNode % destinationNode % startTime;
    const std::string message(std::move(fmt.str()));

    //cgrSock->send(&msg, zmq::send_flags::none);
    std::cout << "Sending CGR request" << std::endl;
    cgrSock->send(zmq::const_buffer(message.data(), message.size()), zmq::send_flags::none);
    std::cout << "Waiting to receive message back" << std::endl;
    zmq::message_t recvMessage;
    cgrSock->recv(recvMessage, zmq::recv_flags::none);

    const std::string myReceivedAsString((const char *)recvMessage.data(), ((const char *)recvMessage.data()) + recvMessage.size());

    //std::cout << "Next hop is " << myReceivedAsString << std::endl;
    return boost::lexical_cast<uint64_t>(myReceivedAsString); //nextNode;
}

