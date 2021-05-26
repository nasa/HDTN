#include <boost/test/unit_test.hpp>
#include "MemoryManagerTree.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <iostream>
#include <string>

BOOST_AUTO_TEST_CASE(FindLsbTestCase)
{
	boost::uint64_t n = 128;
	int a = boost::multiprecision::detail::find_lsb<boost::uint64_t>(n);
	BOOST_REQUIRE_EQUAL(a, 7);
}

#if 0
BOOST_AUTO_TEST_CASE(MemoryManagerTreeTestCase)
{
	
	MemoryManagerTree t;

	t.SetupTree();
	
	//for (boost::uint32_t i = 0; i < 16777216 * 64; ++i) {
	for (boost::uint32_t i = 0; i < 16777216 * 64; ++i) {
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		BOOST_REQUIRE_EQUAL(segmentId, i);
	}
	//std::cout << "testing max\n";
	{
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		BOOST_REQUIRE_EQUAL(segmentId, UINT32_MAX);
	}

	{
		const boost::uint32_t segmentIds[11] = {
			123,
			12345,
			16777 - 43,
			16777,
			16777 + 53,
			16777 + 1234,
			16777 * 2 + 5,
			16777 * 3 + 9,
			16777 * 5 + 2,
			16777 * 9 + 6,
			16777 * 12 + 8
		};

		for (int i = 0; i < 11; ++i) {
			const boost::uint32_t segmentId = segmentIds[i];
			BOOST_REQUIRE(t.FreeSegmentId(segmentId));			
		}
		for (int i = 0; i < 11; ++i) {
			const boost::uint32_t segmentId = segmentIds[i];
			const boost::uint32_t newSegmentId = t.GetAndSetFirstFreeSegmentId();
			BOOST_REQUIRE_EQUAL(newSegmentId, segmentId);			
		}
	}

	{
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		BOOST_REQUIRE_EQUAL(segmentId, UINT32_MAX);		
	}	

	t.FreeTree();
}
#endif
