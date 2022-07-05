#include <boost/test/unit_test.hpp>
#include "libcgr.h"
#include "Environment.h"
#include <boost/algorithm/string.hpp>

BOOST_AUTO_TEST_CASE(DijkstraSimpleTestCase)
{
	std::cout << "Reading contact plan..." << std::endl;
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "cgr" / "test";
	const std::string contactFile = (contactRootDir / "contactPlan_RoutingTest.json").string();

	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	int numContacts = contactPlan.size();
	std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
	BOOST_CHECK(numContacts == 8);

	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_SIZE, 100, 1.0, 0);
	BOOST_REQUIRE(&rootContact);
	rootContact.arrival_time = 0;

	std::cout << "Finding best path using dijkstra's..." << std::endl;
	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 4, contactPlan);
	BOOST_REQUIRE(&bestRoute);
	int nextHop = bestRoute.next_node;
	std::cout << "Next hop is: " << std::to_string(nextHop) << std::endl;
	BOOST_CHECK(nextHop == 2);
}