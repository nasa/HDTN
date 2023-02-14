/**
 * @file TestHdtnDistributedConfig.cpp
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
#include "HdtnDistributedConfig.h"
#include <memory>
#include "Environment.h"
#include <boost/algorithm/string.hpp>

BOOST_AUTO_TEST_CASE(HdtnDistributedConfigTestCase)
{
    const boost::filesystem::path jsonRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "config" / "test";

    HdtnDistributedConfig hdtnDistributedConfig;
    

    BOOST_REQUIRE(hdtnDistributedConfig.ToJsonFile(jsonRootDir / "hdtn_distributed.json"));
    std::string hdtnDistributedJson = hdtnDistributedConfig.ToJson();
    HdtnDistributedConfig_ptr hdtnDistributedConfigFromJsonPtr = HdtnDistributedConfig::CreateFromJson(hdtnDistributedJson);
    BOOST_REQUIRE(hdtnDistributedConfigFromJsonPtr);
    BOOST_REQUIRE(hdtnDistributedConfig == *hdtnDistributedConfigFromJsonPtr);
    BOOST_REQUIRE_EQUAL(hdtnDistributedJson, hdtnDistributedConfigFromJsonPtr->ToJson());
    
}

