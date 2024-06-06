#include <boost/test/unit_test.hpp>

#include "TelemetryRunner.h"
#include "TelemetryDefinitions.h"
#include "Environment.h"


BOOST_AUTO_TEST_CASE(TelemetryRunnerInitTestCase)
{
    TelemetryRunnerProgramOptions options;
    options.m_websocketServerProgramOptions.m_guiPortNumber = "8086";
    options.m_websocketServerProgramOptions.m_guiDocumentRoot = Environment::GetPathGuiDocumentRoot();
    options.m_hdtnDistributedConfigPtr = std::make_shared<HdtnDistributedConfig>(); //default config, needed since inprocContextPtr is null
    TelemetryRunner runner;
    HdtnConfig hdtnConfig;
    BOOST_REQUIRE(runner.Init(hdtnConfig, NULL, options));
    BOOST_REQUIRE_NO_THROW(runner.Stop());
}
