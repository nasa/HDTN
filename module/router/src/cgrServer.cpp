#include "cgrServer.h"
#include "libcgr.h"
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

void CgrServer::init(const std::string & address)
{
    std::cout << "starting init" << std::endl;
    std::vector<cgr::Contact> contact_plan = cgr::cp_load(contactFile, cgr::MAX_SIZE);
    std::cout << "contact plan loaded" << std::endl;
}

uint64_t CgrServer::requestNextHop(uint64_t currentNode, uint64_t destinationNode, uint64_t startTime)
{
    std::cout << "Running dijkstra's from cgrlib" << std::endl;
    cgr::Contact rootContact = cgr::Contact(currentNode, currentNode, 0, cgr::MAX_SIZE, 100, 1.0, 0);
    cgr::Route bestRoute = cgr::dijkstra(&rootContact, destinationNode, contactPlan);
    int nextHop = bestRoute.next_node;
    std::cout << "Next hop is " << nextHop << std::endl;
    return nextHop; //nextNode;
}

