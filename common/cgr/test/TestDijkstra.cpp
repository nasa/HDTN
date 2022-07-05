#include <boost/test/unit_test.hpp>
#include "libcgr.h"
#include "Environment.h"
#include <boost/algorithm/string.hpp>

BOOST_AUTO_TEST_CASE(DijkstraSimpleTestCase)
{
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "cgr" / "test";
	const std::string contactFile = (contactRootDir / "contactPlan.json").string();
	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	//BOOST_REQUIRE(contactPlan);

	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_SIZE, 100, 1.0, 0);
	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 4, contactPlan);
	//BOOST_REQUIRE(bestRoute);
	BOOST_CHECK(bestRoute.next_node == 2);
}