/**
 * @file TestBpSecConfig.cpp
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
#include "BpSecConfig.h"
#include <memory>
#include "Environment.h"
#include <boost/algorithm/string.hpp>

BOOST_AUTO_TEST_CASE(BpSecConfigTestCase)
{
    const boost::filesystem::path jsonRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "config" / "test";

    const boost::filesystem::path jsonFileName = jsonRootDir / "BPSec3.json";
    BpSecConfig_ptr bpsec1 = BpSecConfig::CreateFromJsonFilePath(jsonFileName);
    BOOST_REQUIRE(bpsec1);
    //std::cout << bpsec1->ToJson() << "\n";

    const std::string newJson = boost::trim_copy(bpsec1->ToJson());
    BpSecConfig_ptr bpsec2 = BpSecConfig::CreateFromJson(newJson);
    BOOST_REQUIRE(bpsec2);
    BOOST_REQUIRE(*bpsec2 == *bpsec1);

    std::string fileContentsAsString;
    BOOST_REQUIRE(JsonSerializable::LoadTextFileIntoString(jsonFileName, fileContentsAsString));
    boost::trim(fileContentsAsString);
    BOOST_REQUIRE_EQUAL(fileContentsAsString, newJson);
}

