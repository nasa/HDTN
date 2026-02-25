/**
 * @file TestHdtnDistributedConfig.cpp
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

