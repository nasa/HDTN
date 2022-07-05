#include <boost/test/unit_test.hpp>
#include "libcgr.h"
#include "Environment.h"
#include <boost/algorithm/string.hpp>

BOOST_AUTO_TEST_CASE(DijkstraSimpleTestCase)
{
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "cgr" / "test";
	const std::string contactFile = (contactRootDir / "contactPlan_RoutingTest.json").string();
	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	//BOOST_REQUIRE(contactPlan);
	BOOST_CHECK(contactPlan.size() == 8);

	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_SIZE, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 4, contactPlan);
	//BOOST_REQUIRE(bestRoute);
	int nextHop = bestRoute.next_node;
	std::cout << "Next hop is: " << std::to_string(nextHop) << std::endl;
	BOOST_CHECK(nextHop == 2);
}