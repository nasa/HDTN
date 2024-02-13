/**
 * @file TestHdtnDistributedConfig.cpp
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
#include "HdtnDistributedConfig.h"
#include <memory>
#include "Environment.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>

BOOST_AUTO_TEST_CASE(HdtnDistributedConfigTestCase)
{
    const boost::filesystem::path jsonRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "config" / "test";

    HdtnDistributedConfig hdtnDistributedConfig;
    
    const boost::filesystem::path jsonFileToCreate = jsonRootDir / "hdtn_distributed.json";
    BOOST_REQUIRE(hdtnDistributedConfig.ToJsonFile(jsonFileToCreate));
    std::string hdtnDistributedJson = hdtnDistributedConfig.ToJson();
    HdtnDistributedConfig_ptr hdtnDistributedConfigFromJsonPtr = HdtnDistributedConfig::CreateFromJson(hdtnDistributedJson);
    BOOST_REQUIRE(hdtnDistributedConfigFromJsonPtr);
    BOOST_REQUIRE(hdtnDistributedConfig == *hdtnDistributedConfigFromJsonPtr);
    BOOST_REQUIRE_EQUAL(hdtnDistributedJson, hdtnDistributedConfigFromJsonPtr->ToJson());
    BOOST_REQUIRE(boost::filesystem::remove(jsonFileToCreate));
}

