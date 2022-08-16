#include <boost/test/unit_test.hpp>
#include "libcgr.h"
#include "Environment.h"
#include <boost/algorithm/string.hpp>
#include <iostream>

#include <chrono>

using namespace std;
using namespace std::chrono;

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
	auto start = std::chrono::high_resolution_clock::now();
	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 4, contactPlan);
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;


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
	auto start = std::chrono::high_resolution_clock::now();
	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 5, contactPlan);
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;




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

// Multigraph Routing Tests
// Runs	cmr_dijkstra instead of dijkstra

// test for correct multigraph construction - outdated test
//BOOST_AUTO_TEST_CASE(CMR_DijkstraTestMultigraphConstruction)
//{
//	// Route from node 1 to node 5 using the contact plan from the pyCGR tutorial
//	std::cout << "Reading contact plan..." << std::endl;
//	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "scheduler" / "src";
//	const std::string contactFile = (contactRootDir / "cgrTutorial.json").string();
//
//	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
//	size_t numContacts = contactPlan.size();
//	std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
//	BOOST_CHECK(numContacts == 16);
//
//	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_SIZE, 100, 1.0, 0);
//	rootContact.arrival_time = 0;
//
//	int dest_id = 5;
//
//	cgr::ContactMultigraph cm(contactPlan, dest_id);
//	
//	BOOST_CHECK(cm.vertices.size() == 5);
//	BOOST_CHECK(cm.vertices[1].adjacencies.size() == 3);
//	BOOST_CHECK(cm.vertices[2].adjacencies.size() == 2);
//	BOOST_CHECK(cm.vertices[3].adjacencies.size() == 3);
//	BOOST_CHECK(cm.vertices[4].adjacencies.size() == 2);
//	BOOST_CHECK(cm.vertices[5].adjacencies.size() == 1);
//
//	// check that contacts are sorted correctly
//	std::vector<cgr::Contact> five_to_four = cm.vertices[5].adjacencies[4];
//	BOOST_CHECK(five_to_four[0].start = 0);
//	BOOST_CHECK(five_to_four[1].start = 30);
//	BOOST_CHECK(five_to_four[2].start = 50);
//}

BOOST_AUTO_TEST_CASE(CMR_DijkstraRoutingTestCase)
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
	auto start = std::chrono::high_resolution_clock::now();
	cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 4, contactPlan);
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;

	// Todo: need to finalize what a failed dijkstra search should return
	//BOOST_REQUIRE(&bestRoute);
	cgr::nodeId_t nextHop = bestRoute.next_node;
	BOOST_CHECK(nextHop == 2);

	std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

	int expectedHops = 2;
	std::vector<cgr::Contact> expectedContacts({ contactPlan[1], contactPlan[2] });
	std::vector<cgr::Contact> hops = bestRoute.get_hops();
	BOOST_CHECK(hops.size() == expectedHops);
	for (int i = 0; i < expectedHops; ++i) {
		std::cout << "Expected contact " << i << ": " << expectedContacts[i] << std::endl;
		std::cout << "Actual contact " << i << ": " << hops[i] << std::endl;
		BOOST_CHECK(hops[i] == expectedContacts[i]);
	};
}

BOOST_AUTO_TEST_CASE(CMR_DijkstraPyCGRTutorialTestCase)
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
	auto start = std::chrono::high_resolution_clock::now();
	cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 5, contactPlan);
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;

	//BOOST_REQUIRE(&bestRoute);
	cgr::nodeId_t nextHop = bestRoute.next_node;
	BOOST_CHECK(nextHop == 3);

	std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

	int expectedHops = 3;
	std::vector<cgr::Contact> expectedContacts({ contactPlan[4], contactPlan[6], contactPlan[10] });
	std::vector<cgr::Contact> hops = bestRoute.get_hops();
	BOOST_CHECK(hops.size() == expectedHops);
	for (int i = 0; i < expectedHops; ++i) {
		BOOST_CHECK(hops[i] == expectedContacts[i]);
	};
}

BOOST_AUTO_TEST_CASE(TimingTest_RoutingTest)
{
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "scheduler" / "src";
	const std::string contactFile = (contactRootDir / "cgrTutorial.json").string();
	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_SIZE, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	long cmr_times = 0;
	for (int i = 0; i < 100; ++i) {
		auto cmr_start = std::chrono::high_resolution_clock::now();
		cgr::Route cmr_bestRoute = cgr::cmr_dijkstra(&rootContact, 4, contactPlan);
		auto cmr_end = std::chrono::high_resolution_clock::now();
		auto cmr_duration = duration_cast<microseconds>(cmr_end - cmr_start);

		cmr_times = cmr_times + cmr_duration.count();
	}

	long times = 0;
	for (int i = 0; i < 100; ++i) {
		auto start = std::chrono::high_resolution_clock::now();
		cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 4, contactPlan);
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = duration_cast<microseconds>(end - start);

		times = times + duration.count();
	}

	std::cout << "Dijkstra avg: " << times << std::endl;
	std::cout << "CMR_Dijkstra avg: " << cmr_times << std::endl;

}


BOOST_AUTO_TEST_CASE(TimingTest_Starlink)
{
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "scheduler" / "src";
	const std::string contactFile = (contactRootDir / "starlink.json").string();
	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);

	cgr::Contact rootContact = cgr::Contact(3870, 3870, 0, cgr::MAX_SIZE, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	auto cmr_start = std::chrono::high_resolution_clock::now();
	cgr::Route cmr_bestRoute = cgr::cmr_dijkstra(&rootContact, 2110, contactPlan);
	auto cmr_end = std::chrono::high_resolution_clock::now();
	auto cmr_duration = duration_cast<microseconds>(cmr_end - cmr_start);
	cout << "Time taken by function: "
		<< cmr_duration.count() << " microseconds" << endl;

	auto start = std::chrono::high_resolution_clock::now();
	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 2110, contactPlan);
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;

}

BOOST_AUTO_TEST_CASE(TimingTest_CMConstruction)
{
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "scheduler" / "src";
	const std::string contactFile = (contactRootDir / "cgrTutorial.json").string();
	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_SIZE, 100, 1.0, 0);
	rootContact.arrival_time = 0;


	auto start = std::chrono::high_resolution_clock::now();
	cgr::ContactMultigraph cm(contactPlan, 4);
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;
}