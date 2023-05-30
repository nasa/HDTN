/**
 * @file TestBPSecConfig.cpp
 * @author  Nadia Kortas <nadia.kortas@nasa.gov>
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
#include "BPSecConfig.h"
#include <memory>
#include "Environment.h"
#include <boost/algorithm/string.hpp>

BOOST_AUTO_TEST_CASE(BPSecConfigTestCase)
{
    const boost::filesystem::path jsonRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "config" / "test";

    const boost::filesystem::path jsonFileName = jsonRootDir / "BPSec3.json";
    BPSecConfig_ptr bpsec1 = BPSecConfig::CreateFromJsonFilePath(jsonFileName);
    BOOST_REQUIRE(bpsec1);
    std::cout << bpsec1->ToJson() << "\n";

    
    BPSecConfig bpsecConfig;
    bpsecConfig.m_bpsecConfigName = "my BPSec config";
    BOOST_REQUIRE(bpsecConfig.ToJsonFile(jsonFileName));
    std::string bpsecJson = bpsecConfig.ToJson();
    std::cout << bpsecJson << "\n";
    BPSecConfig_ptr bpsecConfigFromJsonPtr = BPSecConfig::CreateFromJson(bpsecJson);
    BOOST_REQUIRE(bpsecConfigFromJsonPtr);
    BOOST_REQUIRE(bpsecConfig == *bpsecConfigFromJsonPtr);
    BOOST_REQUIRE_EQUAL(bpsecJson, bpsecConfigFromJsonPtr->ToJson());
    BOOST_REQUIRE(boost::filesystem::remove(jsonFileName));

}

