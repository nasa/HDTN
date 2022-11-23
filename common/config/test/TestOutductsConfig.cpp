/**
 * @file TestOutductsConfig.cpp
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
#include "OutductsConfig.h"
#include <memory>
#include "Environment.h"
#include <boost/filesystem/fstream.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>

BOOST_AUTO_TEST_CASE(OutductsConfigTestCase)
{
    const boost::filesystem::path jsonRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "config" / "test";
    const std::string jsonFileName = (jsonRootDir / "outducts.json").string();
    OutductsConfig_ptr oc1 = OutductsConfig::CreateFromJsonFile(jsonFileName);
    BOOST_REQUIRE(oc1);
    const std::string newJson = boost::trim_copy(oc1->ToJson());
    OutductsConfig_ptr oc2 = OutductsConfig::CreateFromJson(newJson);
    BOOST_REQUIRE(oc2);
    BOOST_REQUIRE(*oc2 == *oc1);

    std::ifstream ifs(jsonFileName);

    BOOST_REQUIRE(ifs.good());

    // get length of file:
    ifs.seekg(0, ifs.end);
    std::size_t length = ifs.tellg();
    ifs.seekg(0, ifs.beg);
    std::string jsonFileContentsInMemory(length, ' ');
    ifs.read(&jsonFileContentsInMemory[0], length);
    boost::trim(jsonFileContentsInMemory);

    ifs.close();

    BOOST_REQUIRE_EQUAL(jsonFileContentsInMemory, newJson);




}

