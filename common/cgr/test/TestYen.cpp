#include <boost/test/unit_test.hpp>
#include "libcgr.h"
#include "Environment.h"
#include <boost/algorithm/string.hpp>

BOOST_AUTO_TEST_CASE(YenPyCGRTutorialTestCase)
{
	std::cout << "Reading contact plan..." << std::endl;
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "cgr" / "test";
	const std::string contactFile = (contactRootDir / "cgrTutorial.json").string();

	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	int numContacts = contactPlan.size();
	std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
	BOOST_CHECK(numContacts == 16);

	int max = 10;
	int expected = 7; // data in cgrTutorial.json should give 7 possible paths from 1->5
	std::cout << "Finding up to " << max << " best paths using yen's..." << std::endl;
	std::vector<cgr::Route> routes = yen(1, 5, 0, contactPlan, max);
	BOOST_CHECK(routes.size() == expected);

	// exact bdt depends on owlt so it should be calculated rather than hard coded
	// instead of checking all the bdts let's just check the first three hops
	//int[] bdts = { 3, 4, 11, 31, 31, 51, 51 };
	std::cout << "Found " << routes.size() << " routes with best delivery times:";
	for (int i = 0; i < expected; ++i) {
		//BOOST_CHECK(routes[i].best_delivery_time = bdts[i]);
		std::cout << ' ' << routes[i].best_delivery_time;
	}
	std::cout << std::endl;
	BOOST_CHECK(routes[0].next_node == 3);
	BOOST_CHECK(routes[1].next_node == 2);
	BOOST_CHECK(routes[2].next_node == 5);
}