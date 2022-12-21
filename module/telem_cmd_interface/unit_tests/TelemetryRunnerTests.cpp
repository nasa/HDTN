#include <boost/test/unit_test.hpp>

#include "TelemetryRunner.h"
#include "TelemetryDefinitions.h"
#include "Metrics.h"
#include "Environment.h"


BOOST_AUTO_TEST_CASE(TelemetryRunnerInitTestCase)
{
    TelemetryRunnerProgramOptions options;
    options.m_guiPortNumber = "8086";
    options.m_guiDocumentRoot = (Environment::GetPathHdtnSourceRoot() / "module" / "telem_cmd_interface" / "src" / "gui" ).string();
    TelemetryRunner runner;
    BOOST_REQUIRE_EQUAL(true, runner.Init(NULL, options));
    BOOST_REQUIRE_NO_THROW(runner.Stop());
}
