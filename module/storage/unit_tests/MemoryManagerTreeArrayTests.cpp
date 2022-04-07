#include <boost/test/unit_test.hpp>
#include "MemoryManagerTreeArray.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <iostream>
#include <string>
#include <inttypes.h>

BOOST_AUTO_TEST_CASE(MemoryManagerTreeArrayIsSegmentFreeTestCase)
{
    MemoryManagerTreeArray t(MAX_MEMORY_MANAGER_SEGMENTS - 100);
    memmanager_t backup;
    const segment_id_t segmentId = 7777;

    BOOST_REQUIRE(t.IsSegmentFree(segmentId - 1));
    BOOST_REQUIRE(t.IsSegmentFree(segmentId));
    BOOST_REQUIRE(t.IsSegmentFree(segmentId + 1));

    BOOST_REQUIRE(t.AllocateSegmentId_NotThreadSafe(segmentId));
    BOOST_REQUIRE(!t.AllocateSegmentId_NotThreadSafe(segmentId)); //cannot allocate again

    BOOST_REQUIRE(t.IsSegmentFree(segmentId - 1));
    BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
    BOOST_REQUIRE(t.IsSegmentFree(segmentId + 1));

    t.BackupDataToVector(backup);
    BOOST_REQUIRE(t.IsBackupEqual(backup));

    BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(segmentId));
    BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(segmentId)); //second should fail (already freed)

    BOOST_REQUIRE(!t.IsBackupEqual(backup));

    BOOST_REQUIRE(t.IsSegmentFree(segmentId - 1));
    BOOST_REQUIRE(t.IsSegmentFree(segmentId));
    BOOST_REQUIRE(t.IsSegmentFree(segmentId + 1));

    BOOST_REQUIRE(t.AllocateSegmentId_NotThreadSafe(0));
    BOOST_REQUIRE(!t.AllocateSegmentId_NotThreadSafe(0)); //cannot allocate again
    BOOST_REQUIRE_EQUAL(t.GetAndSetFirstFreeSegmentId_NotThreadSafe(), 1);
    BOOST_REQUIRE(!t.IsSegmentFree(0));
    BOOST_REQUIRE(!t.IsSegmentFree(1));
    BOOST_REQUIRE(t.IsSegmentFree(2));
    BOOST_REQUIRE(t.IsSegmentFree(3));
}

BOOST_AUTO_TEST_CASE(MemoryManagerTreeArrayTestCase)
{
    const uint64_t MAX_SEGMENTS = (1024000000ULL * 8) / SEGMENT_SIZE + 1;
    MemoryManagerTreeArray t(MAX_SEGMENTS);

    //for (segment_id_t i = 0; i < 16777216 * 64; ++i) {
    for (segment_id_t i = 0; i < MAX_SEGMENTS; ++i) {
        BOOST_REQUIRE(t.IsSegmentFree(i));
        const segment_id_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
        BOOST_REQUIRE_EQUAL(segmentId, i);
        //std::cout << "herei " << i << "\n";
        BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
    }
    //std::cout << "testing max\n";
    {
        const segment_id_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
        BOOST_REQUIRE_EQUAL(segmentId, UINT32_MAX);
    }

    {
        const segment_id_t segmentIds[11] = {
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
            const segment_id_t segmentId = segmentIds[i];
            BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
            BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(segmentId));
            BOOST_REQUIRE(t.IsSegmentFree(segmentId));
            BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(segmentId)); //second should fail (already freed)
            BOOST_REQUIRE(t.IsSegmentFree(segmentId)); //still free
        }
        for (int i = 0; i < 11; ++i) {
            const segment_id_t segmentId = segmentIds[i];
            const segment_id_t newSegmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
            BOOST_REQUIRE_EQUAL(newSegmentId, segmentId);
            BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
        }
    }

    {
        const segment_id_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
        BOOST_REQUIRE_EQUAL(segmentId, UINT32_MAX);
    }
}

BOOST_AUTO_TEST_CASE(MemoryManagerTreeArrayTinyTestCase)
{
    {
        const uint64_t MAX_SEGMENTS = 1;
        MemoryManagerTreeArray t(MAX_SEGMENTS);
        
        BOOST_REQUIRE(t.IsSegmentFree(0)); //free
        BOOST_REQUIRE(!t.IsSegmentFree(1)); //out of range
        BOOST_REQUIRE(!t.IsSegmentFree(63)); //out of range
        BOOST_REQUIRE(!t.IsSegmentFree(64)); //out of range
        BOOST_REQUIRE(!t.IsSegmentFree(65)); //out of range
        BOOST_REQUIRE(!t.IsSegmentFree(100000)); //way out of range
        BOOST_REQUIRE(!t.IsSegmentFree(UINT32_MAX)); //way out of range
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(0)); //already free
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(1)); //out of range
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(100000)); //way out of range
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(UINT32_MAX)); //way out of range
        segment_id_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
        BOOST_REQUIRE_EQUAL(segmentId, 0);
        BOOST_REQUIRE(!t.IsSegmentFree(0)); //already allocated
        BOOST_REQUIRE(!t.IsSegmentFree(1)); //out of range

        segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
        BOOST_REQUIRE_EQUAL(segmentId, UINT32_MAX); //already full

        BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(0)); //now freed
        BOOST_REQUIRE(t.IsSegmentFree(0));
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(0)); //already free
    }

    {
        const uint64_t MAX_SEGMENTS = 63;
        MemoryManagerTreeArray t(MAX_SEGMENTS);
    }

    {
        const uint64_t MAX_SEGMENTS = 64;
        MemoryManagerTreeArray t(MAX_SEGMENTS);
    }

    {
        const uint64_t MAX_SEGMENTS = 65;
        MemoryManagerTreeArray t(MAX_SEGMENTS);
        //BOOST_REQUIRE(t.IsSegmentFree(64)); //already free
        //BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(64)); //already free
    }

    {
        const uint64_t MAX_SEGMENTS = 127;
        MemoryManagerTreeArray t(MAX_SEGMENTS);
    }

    {
        const uint64_t MAX_SEGMENTS = 128;
        MemoryManagerTreeArray t(MAX_SEGMENTS);
    }

    {
        const uint64_t MAX_SEGMENTS = 129;
        MemoryManagerTreeArray t(MAX_SEGMENTS);
        //for (segment_id_t i = 0; i < 16777216 * 64; ++i) {
        for (segment_id_t i = 0; i < MAX_SEGMENTS; ++i) {
            //BOOST_REQUIRE(t.IsSegmentFree(i));
            //std::cout << "i " << i << "\n";
            const segment_id_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
            BOOST_REQUIRE_EQUAL(segmentId, i);
            //BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
        }
        std::cout << "testing max\n";
        {
            const segment_id_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
            BOOST_REQUIRE_EQUAL(segmentId, UINT32_MAX);
        }
        /*
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(1));
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(128));
        BOOST_REQUIRE(t.IsSegmentFree(1));
        BOOST_REQUIRE(t.IsSegmentFree(128));
        */
    }

}

BOOST_AUTO_TEST_CASE(MemoryManagerTreeArrayAllocationTestCase)
{
    static const std::vector<std::pair<uint64_t, std::vector<uint64_t> > > maxSegmentsPlusDepthSizesTestCases = {
        {1, {1,1,1,1,1} },
        {63, {1,1,1,1,1} },
        {64, {1,1,1,1,2} },
        {65, {1,1,1,1,2} },
        {127, {1,1,1,1,2} },
        {128, {1,1,1,1,3} },
        {129, {1,1,1,1,3} },
        {(64 * 64) - 1, {1,1,1,1,64} },
        {(64 * 64), {1,1,1,2,65} }
    };
    for(std::size_t testCaseI = 0; testCaseI < maxSegmentsPlusDepthSizesTestCases.size(); ++testCaseI) {
        const std::pair<uint64_t, std::vector<uint64_t> > & testCasePair = maxSegmentsPlusDepthSizesTestCases[testCaseI];
        const uint64_t MAX_SEGMENTS = testCasePair.first;
        MemoryManagerTreeArray t(MAX_SEGMENTS);
        
        const memmanager_t & depthVectors = t.GetVectorsConstRef();
        const std::vector<uint64_t> & expectedDepthSizes = testCasePair.second;
        BOOST_REQUIRE_EQUAL(expectedDepthSizes.size(), MAX_TREE_ARRAY_DEPTH);
        BOOST_REQUIRE_EQUAL(depthVectors.size(), MAX_TREE_ARRAY_DEPTH);
        
        for (std::size_t i = 0; i < MAX_TREE_ARRAY_DEPTH; ++i) {
            BOOST_REQUIRE_EQUAL(expectedDepthSizes[i], depthVectors[i].size());
        }
    }
}
