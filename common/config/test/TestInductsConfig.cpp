/**
 * @file TestInductsConfig.cpp
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
#include "InductsConfig.h"
#include <memory>
#include "Environment.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/fstream.hpp>
#include <fstream>

BOOST_AUTO_TEST_CASE(InductsConfigTestCase)
{
    const boost::filesystem::path jsonRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "config" / "test";
    const boost::filesystem::path jsonFileName = jsonRootDir / "inducts.json";
    InductsConfig_ptr ic1 = InductsConfig::CreateFromJsonFilePath(jsonFileName);
    BOOST_REQUIRE(ic1);
    const std::string newJson = boost::trim_copy(ic1->ToJson());
    InductsConfig_ptr ic2 = InductsConfig::CreateFromJson(newJson);
    BOOST_REQUIRE(ic2);
    BOOST_REQUIRE(*ic2 == *ic1);

    std::string fileContentsAsString;
    BOOST_REQUIRE(JsonSerializable::LoadTextFileIntoString(jsonFileName, fileContentsAsString));
    boost::trim(fileContentsAsString);
    BOOST_REQUIRE_EQUAL(fileContentsAsString, newJson);
}

