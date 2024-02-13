/**
 * @file TestFreeListAllocator.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include <unordered_map>
#include <list>
#include <forward_list>
#include "FreeListAllocator.h"


BOOST_AUTO_TEST_CASE(FreeListAllocatorTestCase)
{
    { //unordered map, fixed size 50
        typedef std::unordered_map<uint64_t, uint64_t,
            std::hash<uint64_t>,
            std::equal_to<uint64_t>,
            FreeListAllocator<std::pair<const uint64_t, uint64_t>, 50 > > uMap_t;
        uMap_t m; //calls allocator, allocates 16 elements (MSVC)
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);
        m.reserve(1000); //calls allocator, allocates 2048 elements (MSVC)
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);

        for (uint64_t i = 0; i < 1000; ++i) {
            m[i] = i + 1;
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        }

        for (uint64_t i = 0; i < 50; ++i) {
            BOOST_REQUIRE_EQUAL(m.erase(i), 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), i + 1);
        }

        for (uint64_t i = 50; i < 120; ++i) {
            BOOST_REQUIRE_EQUAL(m.erase(i), 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 50);
        }

        for (uint64_t i = 0; i < 50; ++i) {
            m[i] = i + 1;
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 50 - (i+1));
            m[i] = i + 2; //unchanged
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 50 - (i + 1));
        }

        for (uint64_t i = 50; i < 120; ++i) {
            m[i] = i + 1;
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        }

        uMap_t m2 = std::move(m); //calls twice FreeListAllocatorDynamic(FreeListAllocatorDynamic&& other) noexcept??
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0); //69?
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetMaxListSize(), 50);

        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);
    }

    { //unordered map, dynamic size with initial 50
        typedef std::unordered_map<uint64_t, uint64_t,
            std::hash<uint64_t>,
            std::equal_to<uint64_t>,
            FreeListAllocatorDynamic<std::pair<const uint64_t, uint64_t>, 50 > > uMapDyn_t;
        uMapDyn_t m, mUnused;
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);
        m.reserve(1000);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);

        //resize list to 60
        m.get_allocator().SetMaxListSizeFromGetAllocatorCopy(60);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);

        //make sure independent of other map
        BOOST_REQUIRE_EQUAL(mUnused.get_allocator().GetMaxListSize(), 50);
        mUnused.get_allocator().SetMaxListSizeFromGetAllocatorCopy(65);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        BOOST_REQUIRE_EQUAL(mUnused.get_allocator().GetMaxListSize(), 65);

        //append 1000
        for (uint64_t i = 0; i < 1000; ++i) {
            m[i] = i + 1;
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        }

        //erase 60
        for (uint64_t i = 0; i < 60; ++i) {
            BOOST_REQUIRE_EQUAL(m.erase(i), 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        }

        //erase 60 more
        for (uint64_t i = 60; i < 120; ++i) {
            BOOST_REQUIRE_EQUAL(m.erase(i), 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 60);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        }

        //resize list to 70
        m.get_allocator().SetMaxListSizeFromGetAllocatorCopy(70);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 60);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);

        //erase 10 more
        for (uint64_t i = 120; i < 130; ++i) {
            BOOST_REQUIRE_EQUAL(m.erase(i), 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), (60 + 1 + i) - 120);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        //erase 10 more
        for (uint64_t i = 130; i < 140; ++i) {
            BOOST_REQUIRE_EQUAL(m.erase(i), 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 70);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        //add 70
        for (uint64_t i = 0; i < 70; ++i) {
            m[i] = i + 1;
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 70 - (i + 1));
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
            m[i] = i + 2; //unchanged
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 70 - (i + 1));
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        //add 50
        for (uint64_t i = 70; i < 120; ++i) {
            m[i] = i + 1;
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        uMapDyn_t m2 = std::move(m); //calls twice FreeListAllocatorDynamic(FreeListAllocatorDynamic&& other) noexcept??
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetMaxListSize(), 70);

        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
    }


    { //list, fixed size 50
        typedef std::list<uint64_t,
            FreeListAllocator<uint64_t, 50 > > list_t;
        list_t m;
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);

        for (uint64_t i = 0; i < 1000; ++i) {
            m.push_back(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        }

        for (uint64_t i = 0; i < 50; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), i + 1);
        }

        for (uint64_t i = 50; i < 120; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 50);
        }

        for (uint64_t i = 0; i < 50; ++i) {
            m.push_back(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 50 - (i + 1));
            m.front() = i + 2; //unchanged
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 50 - (i + 1));
        }

        for (uint64_t i = 50; i < 120; ++i) {
            m.push_back(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        }

        list_t m2 = std::move(m); //calls twice FreeListAllocatorDynamic(FreeListAllocatorDynamic&& other) noexcept??
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetMaxListSize(), 50);

        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);
    }

    { //list, dynamic size with initial 50
        typedef std::list<uint64_t,
            FreeListAllocatorDynamic<uint64_t, 50 > > listDynamic_t;
        listDynamic_t m;
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);

        //resize list to 60
        m.get_allocator().SetMaxListSizeFromGetAllocatorCopy(60);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);

        //append 1000
        for (uint64_t i = 0; i < 1000; ++i) {
            m.push_back(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        }

        //erase 60
        for (uint64_t i = 0; i < 60; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        }

        //erase 60 more
        for (uint64_t i = 60; i < 120; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 60);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        }

        //resize list to 70
        m.get_allocator().SetMaxListSizeFromGetAllocatorCopy(70);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 60);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);

        //erase 10 more
        for (uint64_t i = 120; i < 130; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), (60 + 1 + i) - 120);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        //erase 10 more
        for (uint64_t i = 130; i < 140; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 70);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        //add 70
        for (uint64_t i = 0; i < 70; ++i) {
            m.push_back(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 70 - (i + 1));
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
            m.front() = i + 2; //unchanged
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 70 - (i + 1));
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        //add 50
        for (uint64_t i = 70; i < 120; ++i) {
            m.push_back(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        listDynamic_t m2 = std::move(m); //calls twice FreeListAllocatorDynamic(FreeListAllocatorDynamic&& other) noexcept??
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetMaxListSize(), 70);

        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
    }



    { //forward_list, fixed size 50
        typedef std::forward_list<uint64_t,
            FreeListAllocator<uint64_t, 50 > > list_t;
        list_t m;
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);

        for (uint64_t i = 0; i < 1000; ++i) {
            m.push_front(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        }

        for (uint64_t i = 0; i < 50; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), i + 1);
        }

        for (uint64_t i = 50; i < 120; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 50);
        }

        for (uint64_t i = 0; i < 50; ++i) {
            m.push_front(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 50 - (i + 1));
            m.front() = i + 2; //unchanged
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 50 - (i + 1));
        }

        for (uint64_t i = 50; i < 120; ++i) {
            m.push_front(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        }

        list_t m2 = std::move(m); //calls twice FreeListAllocatorDynamic(FreeListAllocatorDynamic&& other) noexcept??
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetMaxListSize(), 50);

        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);
    }

    { //forward_list, dynamic size with initial 50
        typedef std::forward_list<uint64_t,
            FreeListAllocatorDynamic<uint64_t, 50 > > listDynamic_t;
        listDynamic_t m;
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 50);

        //resize list to 60
        m.get_allocator().SetMaxListSizeFromGetAllocatorCopy(60);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);

        //append 1000
        for (uint64_t i = 0; i < 1000; ++i) {
            m.push_front(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        }

        //erase 60
        for (uint64_t i = 0; i < 60; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        }

        //erase 60 more
        for (uint64_t i = 60; i < 120; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 60);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 60);
        }

        //resize list to 70
        m.get_allocator().SetMaxListSizeFromGetAllocatorCopy(70);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 60);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);

        //erase 10 more
        for (uint64_t i = 120; i < 130; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), (60 + 1 + i) - 120);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        //erase 10 more
        for (uint64_t i = 130; i < 140; ++i) {
            m.pop_front();
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 70);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        //add 70
        for (uint64_t i = 0; i < 70; ++i) {
            m.push_front(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 70 - (i + 1));
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
            m.front() = i + 2; //unchanged
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 70 - (i + 1));
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        //add 50
        for (uint64_t i = 70; i < 120; ++i) {
            m.push_front(i + 1);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
            BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
        }

        listDynamic_t m2 = std::move(m); //calls twice FreeListAllocatorDynamic(FreeListAllocatorDynamic&& other) noexcept??
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m2.get_allocator().GetMaxListSize(), 70);

        BOOST_REQUIRE_EQUAL(m.get_allocator().GetCurrentListSizeFromGetAllocatorCopy(), 0);
        BOOST_REQUIRE_EQUAL(m.get_allocator().GetMaxListSize(), 70);
    }

}

