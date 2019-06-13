#include <boost/test/unit_test.hpp>
#include "MemoryManagerTreeArray.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <iostream>
#include <string>
#include <inttypes.h>

BOOST_AUTO_TEST_CASE(MemoryManagerTreeArrayIsSegmentFreeTestCase)
{
	MemoryManagerTreeArray t;
	backup_memmanager_t backup;
	const segment_id_t segmentId = 7777;

	BOOST_REQUIRE(t.IsSegmentFree(segmentId-1));
	BOOST_REQUIRE(t.IsSegmentFree(segmentId));
	BOOST_REQUIRE(t.IsSegmentFree(segmentId+1));

	t.AllocateSegmentId_NoCheck_NotThreadSafe(segmentId);

	BOOST_REQUIRE(t.IsSegmentFree(segmentId - 1));
	BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
	BOOST_REQUIRE(t.IsSegmentFree(segmentId + 1));

	t.BackupDataToVector(backup);
	BOOST_REQUIRE(t.IsBackupEqual(backup));

	BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(segmentId));

	BOOST_REQUIRE(!t.IsBackupEqual(backup));

	BOOST_REQUIRE(t.IsSegmentFree(segmentId - 1));
	BOOST_REQUIRE(t.IsSegmentFree(segmentId));
	BOOST_REQUIRE(t.IsSegmentFree(segmentId + 1));

	t.AllocateSegmentId_NoCheck_NotThreadSafe(0);
	BOOST_REQUIRE_EQUAL(t.GetAndSetFirstFreeSegmentId_NotThreadSafe(), 1);
	BOOST_REQUIRE(!t.IsSegmentFree(0));
	BOOST_REQUIRE(!t.IsSegmentFree(1));
	BOOST_REQUIRE(t.IsSegmentFree(2));
	BOOST_REQUIRE(t.IsSegmentFree(3));
}

BOOST_AUTO_TEST_CASE(MemoryManagerTreeArrayTestCase)
{
	
	MemoryManagerTreeArray t;
	
	//for (boost::uint32_t i = 0; i < 16777216 * 64; ++i) {
	for (boost::uint32_t i = 0; i < MAX_SEGMENTS; ++i) {
		BOOST_REQUIRE(t.IsSegmentFree(i));
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
		BOOST_REQUIRE_EQUAL(segmentId, i);
		BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
	}
	//std::cout << "testing max\n";
	{
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
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
			BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
			BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(segmentId));	
			BOOST_REQUIRE(t.IsSegmentFree(segmentId));
		}
		for (int i = 0; i < 11; ++i) {
			const boost::uint32_t segmentId = segmentIds[i];
			const boost::uint32_t newSegmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
			BOOST_REQUIRE_EQUAL(newSegmentId, segmentId);		
			BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
		}
	}

	{
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
		BOOST_REQUIRE_EQUAL(segmentId, UINT32_MAX);		
	}	
}
