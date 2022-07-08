#include <boost/test/unit_test.hpp>
#include "libcgr.h"
#include "Environment.h"
#include <boost/algorithm/string.hpp>
#include <iostream>

BOOST_AUTO_TEST_CASE(DijkstraRoutingTestCase)
{
	// Route from node 1 to node 4 using the "RoutingTest" contact plan
	std::cout << "Reading contact plan..." << std::endl;
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "scheduler" / "src";
	const std::string contactFile = (contactRootDir / "contactPlan_RoutingTest.json").string();

	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	size_t numContacts = contactPlan.size();
	std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
	BOOST_CHECK(numContacts == 8);

	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_SIZE, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	std::cout << "Finding best path using dijkstra's..." << std::endl;
	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 4, contactPlan);

	// Todo: need to finalize what a failed dijkstra search should return
	//BOOST_REQUIRE(&bestRoute);
	cgr::nodeId_t nextHop = bestRoute.next_node;
	BOOST_CHECK(nextHop == 2);

	std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

	int expectedHops = 2;
	std::vector<cgr::Contact> expectedContacts({ contactPlan[1], contactPlan[2]});
	std::vector<cgr::Contact> hops = bestRoute.get_hops();
	BOOST_CHECK(hops.size() == expectedHops);
	for (int i = 0; i < expectedHops; ++i) {
		BOOST_CHECK(hops[i] == expectedContacts[i]);
	};
}

BOOST_AUTO_TEST_CASE(DijkstraPyCGRTutorialTestCase)
{
	// Route from node 1 to node 5 using the contact plan from the pyCGR tutorial
	std::cout << "Reading contact plan..." << std::endl;
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "scheduler" / "src";
	const std::string contactFile = (contactRootDir / "cgrTutorial.json").string();

	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	size_t numContacts = contactPlan.size();
	std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
	BOOST_CHECK(numContacts == 16);

	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_SIZE, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	std::cout << "Finding best path using dijkstra's..." << std::endl;
	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 5, contactPlan);
	
	//BOOST_REQUIRE(&bestRoute);
	cgr::nodeId_t nextHop = bestRoute.next_node;
	BOOST_CHECK(nextHop == 3);

	std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

	int expectedHops = 3;
	std::vector<cgr::Contact> expectedContacts({ contactPlan[4], contactPlan[6], contactPlan[10]});
	std::vector<cgr::Contact> hops = bestRoute.get_hops();
	BOOST_CHECK(hops.size() == expectedHops);
	for (int i = 0; i < expectedHops; ++i) {
		BOOST_CHECK(hops[i] == expectedContacts[i]);
	};
}