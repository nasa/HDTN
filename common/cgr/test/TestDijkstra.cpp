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
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
	const boost::filesystem::path contactFile = contactRootDir / "contactPlan_RoutingTest.json";

	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	size_t numContacts = contactPlan.size();
	std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
	BOOST_CHECK(numContacts == 8);

	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	std::cout << "Finding best path using dijkstra's..." << std::endl;
	const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
	cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 4, contactPlan);
	const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;

    BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

	cgr::nodeId_t nextHop = bestRoute.next_node;
	BOOST_CHECK(nextHop == 2);

	std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

	std::size_t expectedHops = 2;
	std::vector<cgr::Contact> expectedContacts({ contactPlan[1], contactPlan[2]});
	std::vector<cgr::Contact> hops = bestRoute.get_hops();
	BOOST_CHECK(hops.size() == expectedHops);
	for (std::size_t i = 0; i < expectedHops; ++i) {
		BOOST_CHECK(hops[i] == expectedContacts[i]);
	};
}

BOOST_AUTO_TEST_CASE(Dijkstra10NodesTestCase)
{
        // Route from node 20 to node 40 using the "10Nodes" contact plan
        std::cout << "Reading contact plan..." << std::endl;
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "10nodes.json";

        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        size_t numContacts = contactPlan.size();
        std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
        BOOST_CHECK(numContacts == 368);

        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        std::cout << "Finding best path using dijkstra's..." << std::endl;
		const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        cgr::Route bestRoute = cgr::dijkstra(&rootContact, 40, contactPlan);
		const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        cout << "Time taken by function: "
                << duration.count() << " microseconds" << endl;

        BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

        cgr::nodeId_t nextHop = bestRoute.next_node;
        BOOST_CHECK(nextHop == 3686);

        std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

}

BOOST_AUTO_TEST_CASE(CMR10NodesTestCase)
{
        // Route from node 20 to node 40 using the "10Nodes" contact plan
        std::cout << "Reading contact plan..." << std::endl;
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "10nodes.json";

        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        size_t numContacts = contactPlan.size();
        std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
        BOOST_CHECK(numContacts == 368);

        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        std::cout << "Finding best path using CMR..." << std::endl;
		const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 40, contactPlan);
		const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        cout << "Time taken by function: "
                << duration.count() << " microseconds" << endl;

        BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

        cgr::nodeId_t nextHop = bestRoute.next_node;
        BOOST_CHECK(nextHop == 3686);

        std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

}


BOOST_AUTO_TEST_CASE(Dijkstra50NodesTestCase)
{
        // Route from node 20 to node 40 using the "50Nodes" contact plan
        std::cout << "Reading contact plan..." << std::endl;
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "50nodes.json";

        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        size_t numContacts = contactPlan.size();
        std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
        BOOST_CHECK(numContacts == 7186);

        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        std::cout << "Finding best path using dijkstra's..." << std::endl;
		const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        cgr::Route bestRoute = cgr::dijkstra(&rootContact, 40, contactPlan);
		const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        cout << "Time taken by function: "
                << duration.count() << " microseconds" << endl;

        BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

        cgr::nodeId_t nextHop = bestRoute.next_node;
        BOOST_CHECK(nextHop == 3545);

        std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

}


BOOST_AUTO_TEST_CASE(CMR50NodesTestCase)
{
        // Route from node 20 to node 40 using the "50Nodes" contact plan
        std::cout << "Reading contact plan..." << std::endl;
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "50nodes.json";

        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        size_t numContacts = contactPlan.size();
        std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
        BOOST_CHECK(numContacts == 7186);

        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        std::cout << "Finding best path using CMR..." << std::endl;
                const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 40, contactPlan);
                const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
                const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        cout << "Time taken by function: "
                << duration.count() << " microseconds" << endl;

        BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

        cgr::nodeId_t nextHop = bestRoute.next_node;
        BOOST_CHECK(nextHop == 3513);

        std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

}

BOOST_AUTO_TEST_CASE(CMR100NodesTestCase)
{
        // Route from node 20 to node 40 using the "100Nodes" contact plan
        std::cout << "Reading contact plan..." << std::endl;
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "100nodes.json";

        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        size_t numContacts = contactPlan.size();
        std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
        BOOST_CHECK(numContacts == 28162);

        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        std::cout << "Finding best path using CMR..." << std::endl;
                const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 40, contactPlan);
                const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
                const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        cout << "Time taken by function: "
                << duration.count() << " microseconds" << endl;

        BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

        cgr::nodeId_t nextHop = bestRoute.next_node;
        BOOST_CHECK(nextHop == 1215);

        std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

}

BOOST_AUTO_TEST_CASE(CMR200NodesTestCase)
{
        // Route from node 20 to node 40 using the "200Nodes" contact plan
        std::cout << "Reading contact plan..." << std::endl;
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "200nodes.json";

        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        size_t numContacts = contactPlan.size();
        std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
        BOOST_CHECK(numContacts == 109329);

        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        std::cout << "Finding best path using CMR..." << std::endl;
                const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 40, contactPlan);
                const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
                const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        cout << "Time taken by function: "
                << duration.count() << " microseconds" << endl;

        BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

        cgr::nodeId_t nextHop = bestRoute.next_node;
        BOOST_CHECK(nextHop == 1270);

        std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

}

BOOST_AUTO_TEST_CASE(Dijkstra100NodesTestCase)
{
        // Route from node 20 to node 40 using the "100Nodes" contact plan
        std::cout << "Reading contact plan..." << std::endl;
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "100nodes.json";

        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        size_t numContacts = contactPlan.size();
        std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
        BOOST_CHECK(numContacts == 28162);

        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        std::cout << "Finding best path using dijkstra's..." << std::endl;
		const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        cgr::Route bestRoute = cgr::dijkstra(&rootContact, 40, contactPlan);
		const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        cout << "Time taken by function: "
                << duration.count() << " microseconds" << endl;

        BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

        cgr::nodeId_t nextHop = bestRoute.next_node;
        BOOST_CHECK(nextHop == 2374);

        std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

}

BOOST_AUTO_TEST_CASE(Dijkstra200NodesTestCase)
{
        // Route from node 20 to node 40 using the "200Nodes" contact plan
        std::cout << "Reading contact plan..." << std::endl;
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "200nodes.json";

        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        size_t numContacts = contactPlan.size();
        std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
        BOOST_CHECK(numContacts == 109329);

        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        std::cout << "Finding best path using dijkstra's..." << std::endl;
		const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        cgr::Route bestRoute = cgr::dijkstra(&rootContact, 40, contactPlan);
		const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        cout << "Time taken by function: "
                << duration.count() << " microseconds" << endl;

        BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

        cgr::nodeId_t nextHop = bestRoute.next_node;
        BOOST_CHECK(nextHop == 1546);

        std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

}

BOOST_AUTO_TEST_CASE(DijkstraRoutingNoPathTestCase)
{
	// Route from node 4 to node 1 using the "RoutingTest" contact plan.
	// Dijkstra's should return a NULL route (i.e. shared nullptr) since no path from
	// 4 to 1 exists in the contact plan.
	std::cout << "Reading contact plan..." << std::endl;
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
	const boost::filesystem::path contactFile = contactRootDir / "contactPlan_RoutingTest.json";

	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);

	cgr::Contact rootContact = cgr::Contact(4, 4, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	std::cout << "Finding best path using dijkstra's..." << std::endl;
	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 1, contactPlan);

    BOOST_REQUIRE(!bestRoute.valid()); // failed dijkstra search

	BOOST_CHECK(bestRoute.get_hops().size() == 0);

	std::cout << "No path found for 4->1. Dijkstra's returned nullptr." << std::endl;
}

BOOST_AUTO_TEST_CASE(DijkstraPyCGRTutorialTestCase)
{
	// Route from node 1 to node 5 using the contact plan from the pyCGR tutorial
	std::cout << "Reading contact plan..." << std::endl;
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
	const boost::filesystem::path contactFile = contactRootDir / "cgrTutorial.json";

	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	size_t numContacts = contactPlan.size();
	std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
	BOOST_CHECK(numContacts == 16);

	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	std::cout << "Finding best path using dijkstra's..." << std::endl;
//	std::shared_ptr<cgr::Route> bestRoute = cgr::dijkstra(&rootContact, 5, contactPlan);
	
//	BOOST_REQUIRE(bestRoute);
//	cgr::nodeId_t nextHop = bestRoute->next_node;
	const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
	cgr::Route bestRoute = cgr::dijkstra(&rootContact, 5, contactPlan);
	const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;

    BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

	//BOOST_REQUIRE(&bestRoute);
	cgr::nodeId_t nextHop = bestRoute.next_node;
	BOOST_CHECK(nextHop == 3);

	std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

    std::size_t expectedHops = 3;
	std::vector<cgr::Contact> expectedContacts({ contactPlan[4], contactPlan[6], contactPlan[10]});
	std::vector<cgr::Contact> hops = bestRoute.get_hops();
	BOOST_CHECK(hops.size() == expectedHops);
	for (std::size_t i = 0; i < expectedHops; ++i) {
		BOOST_CHECK(hops[i] == expectedContacts[i]);
	};
}

// Multigraph Routing Tests
// Runs	cmr_dijkstra instead of dijkstra
BOOST_AUTO_TEST_CASE(CMR_DijkstraRoutingTestCase)
{
	// Route from node 1 to node 4 using the "RoutingTest" contact plan
	std::cout << "Reading contact plan..." << std::endl;
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
	const boost::filesystem::path contactFile = contactRootDir / "contactPlan_RoutingTest.json";

	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	size_t numContacts = contactPlan.size();
	std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
	BOOST_CHECK(numContacts == 8);

	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	std::cout << "Finding best path using dijkstra's..." << std::endl;
	const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
	cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 4, contactPlan);
	const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;

    BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

	cgr::nodeId_t nextHop = bestRoute.next_node;
	BOOST_CHECK(nextHop == 2);

	std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

    std::size_t expectedHops = 2;
	std::vector<cgr::Contact> expectedContacts({ contactPlan[1], contactPlan[2] });
	std::vector<cgr::Contact> hops = bestRoute.get_hops();
	BOOST_CHECK(hops.size() == expectedHops);
	for (std::size_t i = 0; i < expectedHops; ++i) {
		std::cout << "Expected contact " << i << ": " << expectedContacts[i] << std::endl;
		std::cout << "Actual contact " << i << ": " << hops[i] << std::endl;
		BOOST_CHECK(hops[i] == expectedContacts[i]);
	};
}

BOOST_AUTO_TEST_CASE(CMR_DijkstraPyCGRTutorialTestCase)
{
	// Route from node 1 to node 5 using the contact plan from the pyCGR tutorial
	std::cout << "Reading contact plan..." << std::endl;
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
	const boost::filesystem::path contactFile = contactRootDir / "cgrTutorial.json";

	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	size_t numContacts = contactPlan.size();
	std::cout << "Contact plan with " << numContacts << " contacts read" << std::endl;
	BOOST_CHECK(numContacts == 16);

	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	std::cout << "Finding best path using dijkstra's..." << std::endl;
	const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
	cgr::Route bestRoute = cgr::cmr_dijkstra(&rootContact, 5, contactPlan);
	const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	cout << "Time taken by function: "
		<< duration.count() << " microseconds" << endl;

    BOOST_REQUIRE(bestRoute.valid()); // not a failed dijkstra search

	cgr::nodeId_t nextHop = bestRoute.next_node;
	BOOST_CHECK(nextHop == 3);

	std::cout << "Route found (next hop is " << nextHop << "):" << std::endl << bestRoute << std::endl;

    std::size_t expectedHops = 3;
	std::vector<cgr::Contact> expectedContacts({ contactPlan[4], contactPlan[6], contactPlan[10] });
	std::vector<cgr::Contact> hops = bestRoute.get_hops();
	BOOST_CHECK(hops.size() == expectedHops);
	for (std::size_t i = 0; i < expectedHops; ++i) {
		BOOST_CHECK(hops[i] == expectedContacts[i]);
	};
}

// Ad hoc timing tests

BOOST_AUTO_TEST_CASE(TimingTest_RoutingTest, *boost::unit_test::disabled())
{
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
	const boost::filesystem::path contactFile = contactRootDir / "contactPlan_RoutingTest.json";
	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	long long cmr_times = 0;
	for (int i = 0; i < 100; ++i) {
		const std::chrono::high_resolution_clock::time_point cmr_start = std::chrono::high_resolution_clock::now();
		cgr::Route cmr_bestRoute = cgr::cmr_dijkstra(&rootContact, 4, contactPlan);
		const std::chrono::high_resolution_clock::time_point cmr_end = std::chrono::high_resolution_clock::now();
		const std::chrono::microseconds cmr_duration = std::chrono::duration_cast<std::chrono::microseconds>(cmr_end - cmr_start);

		cmr_times = cmr_times + cmr_duration.count();
	}

	long long times = 0;
	for (int i = 0; i < 100; ++i) {
		const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
		cgr::Route bestRoute = cgr::dijkstra(&rootContact, 4, contactPlan);
		const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

		times = times + duration.count();
	}

	std::cout << "Dijkstra avg: " << times / 100 << std::endl;
	std::cout << "CMR_Dijkstra avg: " << cmr_times / 100 << std::endl;

}

BOOST_AUTO_TEST_CASE(TimingTest_10nodesTest, *boost::unit_test::disabled())
{
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "10nodes.json";
        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        long long cmr_times = 0;
        for (int i = 0; i < 100; ++i) {
                const std::chrono::high_resolution_clock::time_point cmr_start = std::chrono::high_resolution_clock::now();
                cgr::Route cmr_bestRoute = cgr::cmr_dijkstra(&rootContact, 40, contactPlan);
                const std::chrono::high_resolution_clock::time_point cmr_end = std::chrono::high_resolution_clock::now();
                const std::chrono::microseconds cmr_duration = std::chrono::duration_cast<std::chrono::microseconds>(cmr_end - cmr_start);

                cmr_times = cmr_times + cmr_duration.count();
        }

        long long times = 0;
        for (int i = 0; i < 100; ++i) {
                const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
                cgr::Route bestRoute = cgr::dijkstra(&rootContact, 40, contactPlan);
                const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
                const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

                times = times + duration.count();
        }

        std::cout << "Dijkstra avg: " << times / 100 << std::endl;
        std::cout << "CMR_Dijkstra avg: " << cmr_times / 100 << std::endl;

}


BOOST_AUTO_TEST_CASE(TimingTest_50nodesTest, *boost::unit_test::disabled())
{
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "50nodes.json";
        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        long long cmr_times = 0;
        for (int i = 0; i < 100; ++i) {
                const std::chrono::high_resolution_clock::time_point cmr_start = std::chrono::high_resolution_clock::now();
                cgr::Route cmr_bestRoute = cgr::cmr_dijkstra(&rootContact, 40, contactPlan);
                const std::chrono::high_resolution_clock::time_point cmr_end = std::chrono::high_resolution_clock::now();
                const std::chrono::microseconds cmr_duration = std::chrono::duration_cast<std::chrono::microseconds>(cmr_end - cmr_start);

                cmr_times = cmr_times + cmr_duration.count();
        }

        long long times = 0;
        for (int i = 0; i < 100; ++i) {
                const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
                cgr::Route bestRoute = cgr::dijkstra(&rootContact, 40, contactPlan);
                const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
                const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

                times = times + duration.count();
        }

        std::cout << "Dijkstra avg: " << times / 100 << std::endl;
        std::cout << "CMR_Dijkstra avg: " << cmr_times / 100 << std::endl;

}

BOOST_AUTO_TEST_CASE(TimingTest_100nodesTest, *boost::unit_test::disabled())
{
        const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
        const boost::filesystem::path contactFile = contactRootDir / "100nodes.json";
        std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
        cgr::Contact rootContact = cgr::Contact(20, 20, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
        rootContact.arrival_time = 0;

        long long cmr_times = 0;
        for (int i = 0; i < 100; ++i) {
                const std::chrono::high_resolution_clock::time_point cmr_start = std::chrono::high_resolution_clock::now();
                cgr::Route cmr_bestRoute = cgr::cmr_dijkstra(&rootContact, 40, contactPlan);
                const std::chrono::high_resolution_clock::time_point cmr_end = std::chrono::high_resolution_clock::now();
                const std::chrono::microseconds cmr_duration = std::chrono::duration_cast<std::chrono::microseconds>(cmr_end - cmr_start);

                cmr_times = cmr_times + cmr_duration.count();
        }

        long long times = 0;
        for (int i = 0; i < 100; ++i) {
                const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
                cgr::Route bestRoute = cgr::dijkstra(&rootContact, 40, contactPlan);
                const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
                const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

                times = times + duration.count();
        }

        std::cout << "Dijkstra avg: " << times / 100 << std::endl;
        std::cout << "CMR_Dijkstra avg: " << cmr_times / 100 << std::endl;

}


BOOST_AUTO_TEST_CASE(TimingTest_CMConstruction, *boost::unit_test::disabled())
{
	const boost::filesystem::path contactRootDir = Environment::GetPathHdtnSourceRoot() / "module" / "router" / "contact_plans";
	const boost::filesystem::path contactFile = contactRootDir / "contactPlan_RoutingTest.json";
	std::vector<cgr::Contact> contactPlan = cgr::cp_load(contactFile);
	cgr::Contact rootContact = cgr::Contact(1, 1, 0, cgr::MAX_TIME_T, 100, 1.0, 0);
	rootContact.arrival_time = 0;

	long long times = 0;
	for (int i = 0; i < 100; ++i) {
		const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
		cgr::ContactMultigraph cm(contactPlan, 4);
		const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		const std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		times = times + duration.count();
	}

	std::cout << "Construction avg: " << times / 100 << std::endl;
}
