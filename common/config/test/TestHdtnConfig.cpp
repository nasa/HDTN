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

    const std::string jsonFileNameInducts = (jsonRootDir / "inducts.json").string();
    InductsConfig_ptr ic1 = InductsConfig::CreateFromJsonFile(jsonFileNameInducts);
    BOOST_REQUIRE(ic1);
    hdtnConfig.m_inductsConfig = std::move(*ic1);

    const std::string jsonFileNameOutducts = (jsonRootDir / "outducts.json").string();
    OutductsConfig_ptr oc1 = OutductsConfig::CreateFromJsonFile(jsonFileNameOutducts);
    BOOST_REQUIRE(oc1);
    hdtnConfig.m_outductsConfig = std::move(*oc1);

    const std::string jsonFileNameStorage = (jsonRootDir / "storage.json").string();
    StorageConfig_ptr s1 = StorageConfig::CreateFromJsonFile(jsonFileNameStorage);
    BOOST_REQUIRE(s1);
    hdtnConfig.m_storageConfig = std::move(*s1);

    BOOST_REQUIRE(hdtnConfig.ToJsonFile((jsonRootDir / "hdtn.json").string()));
    std::string hdtnJson = hdtnConfig.ToJson();
    HdtnConfig_ptr hdtnConfigFromJsonPtr = HdtnConfig::CreateFromJson(hdtnJson);
    BOOST_REQUIRE(hdtnConfigFromJsonPtr);
    BOOST_REQUIRE(hdtnConfig == *hdtnConfigFromJsonPtr);
    BOOST_REQUIRE_EQUAL(hdtnJson, hdtnConfigFromJsonPtr->ToJson());
    
}

