/**
 * @file TestOutductsConfig.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
    const boost::filesystem::path jsonFileName = jsonRootDir / "outducts.json";
    OutductsConfig_ptr oc1 = OutductsConfig::CreateFromJsonFilePath(jsonFileName);
    BOOST_REQUIRE(oc1);
  //  std::cout << oc1->ToJson() << "\n";
    const std::string newJson = boost::trim_copy(oc1->ToJson());
    OutductsConfig_ptr oc2 = OutductsConfig::CreateFromJson(newJson);
    BOOST_REQUIRE(oc2);
    BOOST_REQUIRE(*oc2 == *oc1);

    std::string fileContentsAsString;
    BOOST_REQUIRE(JsonSerializable::LoadTextFileIntoString(jsonFileName, fileContentsAsString));
    boost::trim(fileContentsAsString);
    BOOST_REQUIRE_EQUAL(fileContentsAsString, newJson);
}

BOOST_AUTO_TEST_CASE(OutductsConfigRatePrecisionMicroSecTestCase)
{
  const boost::filesystem::path jsonRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "config" / "test";
  const boost::filesystem::path jsonFileName = jsonRootDir / "outducts.json";
  OutductsConfig_ptr oc1 = OutductsConfig::CreateFromJsonFilePath(jsonFileName);
  BOOST_REQUIRE(oc1);
  BOOST_REQUIRE_EQUAL(oc1->m_outductElementConfigVector.size(), 8);

  // Verify a value is parsed when the field exists
  BOOST_REQUIRE_EQUAL(oc1->m_outductElementConfigVector[0].rateLimitPrecisionMicroSec, 500);
  BOOST_REQUIRE_EQUAL(oc1->m_outductElementConfigVector[1].rateLimitPrecisionMicroSec, 500);

  // Verify the default is used when the field is missing
  for (uint64_t i = 2; i < oc1->m_outductElementConfigVector.size(); i++) {
    BOOST_REQUIRE_EQUAL(oc1->m_outductElementConfigVector[i].rateLimitPrecisionMicroSec, 100000);
  }
}
