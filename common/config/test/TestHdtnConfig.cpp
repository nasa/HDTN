/**
 * @file TestHdtnConfig.cpp
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
#include "HdtnConfig.h"
#include <memory>
#include "Environment.h"
#include <boost/algorithm/string.hpp>

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

