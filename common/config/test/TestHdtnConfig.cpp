/**
 * @file TestHdtnConfig.cpp
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
#include "HdtnConfig.h"
#include <memory>
#include "Environment.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>

BOOST_AUTO_TEST_CASE(HdtnConfigTestCase)
{
    const boost::filesystem::path jsonRootDir = Environment::GetPathHdtnSourceRoot() / "common" / "config" / "test";

    HdtnConfig hdtnConfig;
    hdtnConfig.m_hdtnConfigName = "my hdtn config";
    hdtnConfig.m_myNodeId = 10;

    const boost::filesystem::path jsonFileNameInducts = jsonRootDir / "inducts.json";
    InductsConfig_ptr ic1 = InductsConfig::CreateFromJsonFilePath(jsonFileNameInducts);
    BOOST_REQUIRE(ic1);
    hdtnConfig.m_inductsConfig = std::move(*ic1);

    const boost::filesystem::path jsonFileNameOutducts = jsonRootDir / "outducts.json";
    OutductsConfig_ptr oc1 = OutductsConfig::CreateFromJsonFilePath(jsonFileNameOutducts);
    BOOST_REQUIRE(oc1);
    hdtnConfig.m_outductsConfig = std::move(*oc1);

    const boost::filesystem::path jsonFileNameStorage = jsonRootDir / "storage.json";
    StorageConfig_ptr s1 = StorageConfig::CreateFromJsonFilePath(jsonFileNameStorage);
    BOOST_REQUIRE(s1);
    hdtnConfig.m_storageConfig = std::move(*s1);

    const boost::filesystem::path jsonFileToCreate = jsonRootDir / "hdtn.json";
    BOOST_REQUIRE(hdtnConfig.ToJsonFile(jsonFileToCreate));
    std::string hdtnJson = hdtnConfig.ToJson();
    HdtnConfig_ptr hdtnConfigFromJsonPtr = HdtnConfig::CreateFromJson(hdtnJson);
    BOOST_REQUIRE(hdtnConfigFromJsonPtr);
    BOOST_REQUIRE(hdtnConfig == *hdtnConfigFromJsonPtr);
    BOOST_REQUIRE_EQUAL(hdtnJson, hdtnConfigFromJsonPtr->ToJson());
    BOOST_REQUIRE(boost::filesystem::remove(jsonFileToCreate));
}

