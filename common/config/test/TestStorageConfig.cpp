/**
 * @file TestStorageConfig.cpp
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
#include "StorageConfig.h"
#include <memory>

BOOST_AUTO_TEST_CASE(StorageConfigTestCase)
{
    StorageConfig_ptr sc1 = std::make_shared< StorageConfig>();
    sc1->m_totalStorageCapacityBytes = 100000;
    sc1->AddDisk("d1", "/mnt/d1/d1.bin");
    sc1->AddDisk("d2", "/mnt/d2/d2.bin");
    //sc1->ToJsonFile("storageConfig.json");

    StorageConfig_ptr sc1_copy = std::make_shared< StorageConfig>();
    sc1_copy->m_totalStorageCapacityBytes = 100000;
    sc1_copy->AddDisk("d1", "/mnt/d1/d1.bin");
    sc1_copy->AddDisk("d2", "/mnt/d2/d2.bin");

    StorageConfig_ptr sc2 = std::make_shared< StorageConfig>();
    sc2->m_totalStorageCapacityBytes = 100000;
    sc2->AddDisk("d0", "/mnt/d0/d0.bin");
    sc2->AddDisk("d1", "/mnt/d0/d0.bin");

    StorageConfig sc2StackCopy = *sc2; //copy assignment


    BOOST_REQUIRE(*sc1 == *sc1_copy);
    BOOST_REQUIRE(!(*sc1 == *sc2));
    BOOST_REQUIRE(*sc2 == sc2StackCopy);
    StorageConfig sc2Moved = std::move(sc2StackCopy); //move assignment
    BOOST_REQUIRE(!(*sc2 == sc2StackCopy)); //sc2StackCopy should be changed from move
    BOOST_REQUIRE(*sc2 == sc2Moved);

    std::string sc1Json = sc1->ToJson();
    //std::cout << sc1Json << "\n";
    StorageConfig_ptr sc1_fromJson = StorageConfig::CreateFromJson(sc1Json);
    BOOST_REQUIRE(sc1_fromJson); //not null
    BOOST_REQUIRE(*sc1 == *sc1_fromJson);
    BOOST_REQUIRE(sc1Json == sc1_fromJson->ToJson());
    BOOST_REQUIRE_EQUAL(sc1_fromJson->m_storageDiskConfigVector.size(), 2);
    BOOST_REQUIRE_EQUAL(sc1_fromJson->m_totalStorageCapacityBytes, 100000);

}

