#include <boost/asio.hpp>
#include <cstdint>
#include "HdtnConfig.h"
#include "JsonSerializable.h"
#include <boost/make_unique.hpp>
#include <zmq.hpp>
#include "libcgr.h"

class CgrServer
{
    public:
    std::unique_ptr<zmq::context_t> cgrCtx;
    std::unique_ptr<zmq::socket_t> cgrSock;
    std::string contactFile;
    std::vector<cgr::Contact> contactPlan;

    /*
    Initializes server and establishes socket connection to py_cgr

    std::string address: IP address of py_cgr listener
    */
    void init(const std::string & address);

    /*
    Sends a request to py_cgr over zmq and listens to for a response.

    int currentNode: ID of node. Starting location of bundle path.
    int destinationNode: Final destination of bundle
    int startTime: Time search should begin based on cgrTable

    returns ID of the next node according to shortest path using Dijkstra's algorithm.
    */
    uint64_t requestNextHop(uint64_t currentNode, uint64_t destinationNode, uint64_t startTime);
};
