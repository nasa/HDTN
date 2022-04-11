/**
 * @file MemoryManagerTreeArrayTests.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

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
        BOOST_REQUIRE_EQUAL(segmentId, SEGMENT_ID_FULL);
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
        BOOST_REQUIRE_EQUAL(segmentId, SEGMENT_ID_FULL);
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
        BOOST_REQUIRE(!t.IsSegmentFree(SEGMENT_ID_FULL)); //way out of range
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(0)); //already free
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(1)); //out of range
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(100000)); //way out of range
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(SEGMENT_ID_FULL)); //way out of range
        segment_id_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
        BOOST_REQUIRE_EQUAL(segmentId, 0);
        BOOST_REQUIRE(!t.IsSegmentFree(0)); //already allocated
        BOOST_REQUIRE(!t.IsSegmentFree(1)); //out of range

        segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
        BOOST_REQUIRE_EQUAL(segmentId, SEGMENT_ID_FULL); //already full

        BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(0)); //now freed
        BOOST_REQUIRE(t.IsSegmentFree(0));
        BOOST_REQUIRE(!t.FreeSegmentId_NotThreadSafe(0)); //already free
    }

    {
        const uint64_t MAX_SEGMENTS = 129;
        MemoryManagerTreeArray t(MAX_SEGMENTS);
        for (segment_id_t i = 0; i < MAX_SEGMENTS; ++i) {
            BOOST_REQUIRE(t.IsSegmentFree(i));
            const segment_id_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
            BOOST_REQUIRE_EQUAL(segmentId, i);
            BOOST_REQUIRE(!t.IsSegmentFree(segmentId));
        }
        //std::cout << "testing max\n";
        {
            const segment_id_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
            BOOST_REQUIRE_EQUAL(segmentId, SEGMENT_ID_FULL);
        }
        
        BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(1));
        BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(128));
        BOOST_REQUIRE(t.IsSegmentFree(1));
        BOOST_REQUIRE(t.IsSegmentFree(128));
        
    }

}

BOOST_AUTO_TEST_CASE(MemoryManagerTreeArrayAllocationTestCase)
{
    static const std::vector<std::pair<uint64_t, std::vector<uint64_t> > > maxSegmentsPlusDepthSizesTestCases = {
        {1, {1,1,1,1,1,1} },
        {63, {1,1,1,1,1,1} },
        {64, {1,1,1,1,1,2} },
        {65, {1,1,1,1,1,2} },
        {127, {1,1,1,1,1,2} },
        {128, {1,1,1,1,1,3} },
        {129, {1,1,1,1,1,3} },
        {(64 * 64) - 1, {1,1,1,1,1,64} },
        {(64 * 64), {1,1,1,1,2,65} },
        {(64 * 64 * 64) - 1, {1,1,1,1,64,64*64} },
        {(64 * 64 * 64), {1,1,1,2,65,64*64 + 1} },
        {(64 * 64 * 64 * 64) - 1, {1,1,1,64,64 * 64,64 * 64 * 64} },
        {(64 * 64 * 64 * 64), {1,1,2,65,64*64+1,64 * 64 * 64 + 1} },
        {(64 * 64 * 64 * 64 * 64) - 1, {1,1,64,64*64,64 * 64*64,64 * 64 * 64*64} }/*,
        {(64 * 64 * 64 * 64), {1,2,65,64 * 64 + 1,64 * 64 * 64 + 1} }*/
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

BOOST_AUTO_TEST_CASE(MemoryManagerTreeArrayFullTestCase)
{
    //const uint64_t MAX_SEGMENTS = MAX_MEMORY_MANAGER_SEGMENTS;
    //Test case "MemoryManagerTreeArrayFullTestCase" has passed with:
    //4294967240 assertions out of 4294967240 passed
    //took 2hrs with depth 5 and max segments of (1073741824 - 1)

    const uint64_t MAX_SEGMENTS = (102400000ULL * 8) / SEGMENT_SIZE + 11;
    MemoryManagerTreeArray t(MAX_SEGMENTS);
    for (segment_id_t i = 0; i < (MAX_SEGMENTS - 1); ++i) {

        BOOST_REQUIRE(t.IsSegmentFree(i));

        for (unsigned int count = 0; count < 2; ++count) {
            //get and free next segment
            BOOST_REQUIRE_EQUAL(t.GetAndSetFirstFreeSegmentId_NotThreadSafe(), i);
            BOOST_REQUIRE(!t.IsSegmentFree(i));
            BOOST_REQUIRE(t.IsSegmentFree(i + 1));
            BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(i));

            BOOST_REQUIRE(t.IsSegmentFree(i));
        }

        for (unsigned int count = 0; count < 2; ++count) {
            //manually allocate i
            BOOST_REQUIRE(t.AllocateSegmentId_NotThreadSafe(i));

            //get (i+1) and free (i and i+1) segments again
            BOOST_REQUIRE_EQUAL(t.GetAndSetFirstFreeSegmentId_NotThreadSafe(), i + 1);
            BOOST_REQUIRE(!t.IsSegmentFree(i));
            BOOST_REQUIRE(!t.IsSegmentFree(i + 1));
            BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(i));
            BOOST_REQUIRE(t.FreeSegmentId_NotThreadSafe(i + 1));

            BOOST_REQUIRE(t.IsSegmentFree(i));
            BOOST_REQUIRE(t.IsSegmentFree(i + 1));
        }

        BOOST_REQUIRE(t.AllocateSegmentId_NotThreadSafe(i));
    }
}
