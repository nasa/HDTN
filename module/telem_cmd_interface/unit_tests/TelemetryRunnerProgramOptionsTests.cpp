#include <boost/test/unit_test.hpp>
#include "TelemetryRunnerProgramOptions.h"
#include "Environment.h"

#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
BOOST_AUTO_TEST_CASE(TelemetryRunnerProgramOptionsAppendToDescTestCase)
{
    boost::program_options::options_description desc;
    TelemetryRunnerProgramOptions::AppendToDesc(desc);
#ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
    BOOST_REQUIRE_EQUAL(6, desc.options().size());
#else
    BOOST_REQUIRE_EQUAL(2, desc.options().size());
#endif

    BOOST_REQUIRE_EQUAL("document-root", (desc.options()[0]).get()->long_name());
    BOOST_REQUIRE_EQUAL("port-number", (desc.options()[1]).get()->long_name());
}
#endif

#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
BOOST_AUTO_TEST_CASE(TelemetryRunnerProgramOptionsDefaultTestCase)
{
    boost::program_options::options_description desc("Allowed options");
    TelemetryRunnerProgramOptions::AppendToDesc(desc);
    boost::program_options::variables_map vm;
    int argc = 1; //cannot be size 0
    const char *const argv[1] = {"unit_test"}; //cannot be size 0
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive),
        vm);
    boost::program_options::notify(vm);

    boost::filesystem::path defaultRoot = Environment::GetPathHdtnSourceRoot() / "module" / "telem_cmd_interface" / "src" / "gui";
    TelemetryRunnerProgramOptions options;
    options.ParseFromVariableMap(vm);
    BOOST_REQUIRE_EQUAL(defaultRoot, options.m_guiDocumentRoot);
    BOOST_REQUIRE_EQUAL("8086", options.m_guiPortNumber);
}
#endif

#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
BOOST_AUTO_TEST_CASE(TelemetryRunnerProgramOptionsParseFromVmTestCase)
{
    boost::program_options::variables_map vm;
    boost::filesystem::path validRoot = Environment::GetPathHdtnSourceRoot() / "module" / "telem_cmd_interface" / "src" / "gui";
    vm.insert(std::make_pair("document-root", boost::program_options::variable_value(validRoot, false)));
    vm.insert(std::make_pair("port-number", boost::program_options::variable_value(uint16_t(9000), false)));

    TelemetryRunnerProgramOptions options;
    options.ParseFromVariableMap(vm);
    BOOST_REQUIRE_EQUAL(validRoot, options.m_guiDocumentRoot);
    BOOST_REQUIRE_EQUAL("9000", options.m_guiPortNumber);
}
#endif

#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
BOOST_AUTO_TEST_CASE(TelemetryRunnerProgramOptionsDocumentRootTestCase)
{
    boost::program_options::variables_map vm;
    TelemetryRunnerProgramOptions options;

    // It should handle a valid path
    boost::filesystem::path validRoot = Environment::GetPathHdtnSourceRoot() / "module" / "telem_cmd_interface" / "src" / "gui";
    vm.insert(std::make_pair("document-root", boost::program_options::variable_value(validRoot, false)));
    options.ParseFromVariableMap(vm);
    BOOST_REQUIRE_EQUAL(validRoot, options.m_guiDocumentRoot);

    // It should handle an invalid path
    vm.clear();
    options.m_guiDocumentRoot = "";
    vm.insert(std::make_pair("document-root", boost::program_options::variable_value("foobar", false)));
    options.ParseFromVariableMap(vm);
    BOOST_REQUIRE_EQUAL("", options.m_guiDocumentRoot);

    // It should handle an invalid value
    vm.clear();
    options.m_guiDocumentRoot = "";
    vm.insert(std::make_pair("document-root", boost::program_options::variable_value(100, false)));
    options.ParseFromVariableMap(vm);
    BOOST_REQUIRE_EQUAL("", options.m_guiDocumentRoot);
}
#endif

#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
BOOST_AUTO_TEST_CASE(TelemetryRunnerProgramOptionsPortNumberTestCase)
{
    boost::program_options::variables_map vm;
    TelemetryRunnerProgramOptions options;

    // It should handle a valid integer
    vm.insert(std::make_pair("port-number", boost::program_options::variable_value(uint16_t(8000), false)));
    options.ParseFromVariableMap(vm);
    BOOST_REQUIRE_EQUAL("8000", options.m_guiPortNumber);

    // It should handle an invalid value
    vm.clear();
    options.m_guiPortNumber = "";
    vm.insert(std::make_pair("port-number", boost::program_options::variable_value("foobar", false)));
    options.ParseFromVariableMap(vm);
    BOOST_REQUIRE_EQUAL("", options.m_guiPortNumber);
}
#endif
