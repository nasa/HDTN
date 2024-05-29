#include <boost/test/unit_test.hpp>
#include "TelemetryRunnerProgramOptions.h"
#include "Environment.h"

BOOST_AUTO_TEST_CASE(TelemetryRunnerProgramOptionsAppendToDescTestCase)
{
    boost::program_options::options_description desc;
    TelemetryRunnerProgramOptions::AppendToDesc(desc);
    if (WebsocketServer::IsCompiledWithSsl()) {
        BOOST_REQUIRE_EQUAL(6, desc.options().size());
    }
    else if (WebsocketServer::IsCompiled()) {
        BOOST_REQUIRE_EQUAL(2, desc.options().size());
    }
    else {
        BOOST_REQUIRE_EQUAL(0, desc.options().size());
    }

    if (WebsocketServer::IsCompiled()) {
        BOOST_REQUIRE_EQUAL("document-root", (desc.options()[0]).get()->long_name());
        BOOST_REQUIRE_EQUAL("port-number", (desc.options()[1]).get()->long_name());
    }
}

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


    
    TelemetryRunnerProgramOptions options;
    BOOST_REQUIRE(options.ParseFromVariableMap(vm));
    if (WebsocketServer::IsCompiled()) {
        boost::filesystem::path defaultRoot = Environment::GetPathGuiDocumentRoot();
        BOOST_REQUIRE_EQUAL(defaultRoot, options.m_websocketServerProgramOptions.m_guiDocumentRoot);
        BOOST_REQUIRE_EQUAL("8086", options.m_websocketServerProgramOptions.m_guiPortNumber);
    }
    else {
        BOOST_REQUIRE_EQUAL("", options.m_websocketServerProgramOptions.m_guiDocumentRoot);
        BOOST_REQUIRE_EQUAL("", options.m_websocketServerProgramOptions.m_guiPortNumber);
    }
}

BOOST_AUTO_TEST_CASE(TelemetryRunnerProgramOptionsParseFromVmTestCase)
{
    boost::program_options::variables_map vm;
    boost::filesystem::path validRoot = Environment::GetPathGuiDocumentRoot();
    vm.insert(std::make_pair("document-root", boost::program_options::variable_value(validRoot, false)));
    vm.insert(std::make_pair("port-number", boost::program_options::variable_value(uint16_t(9000), false)));

    TelemetryRunnerProgramOptions options;
    BOOST_REQUIRE(options.ParseFromVariableMap(vm));
    if (WebsocketServer::IsCompiled()) {
        BOOST_REQUIRE_EQUAL(validRoot, options.m_websocketServerProgramOptions.m_guiDocumentRoot);
        BOOST_REQUIRE_EQUAL("9000", options.m_websocketServerProgramOptions.m_guiPortNumber);
    }
    else {
        BOOST_REQUIRE_EQUAL("", options.m_websocketServerProgramOptions.m_guiDocumentRoot);
        BOOST_REQUIRE_EQUAL("", options.m_websocketServerProgramOptions.m_guiPortNumber);
    }
}

BOOST_AUTO_TEST_CASE(TelemetryRunnerProgramOptionsDocumentRootTestCase)
{
    boost::program_options::variables_map vm;
    TelemetryRunnerProgramOptions options;

    // It should handle a valid path
    boost::filesystem::path validRoot = Environment::GetPathGuiDocumentRoot();
    vm.insert(std::make_pair("document-root", boost::program_options::variable_value(validRoot, false)));
    BOOST_REQUIRE(!options.ParseFromVariableMap(vm)); //fails because port is missing but still sets document root
    BOOST_REQUIRE_EQUAL(options.m_websocketServerProgramOptions.m_guiDocumentRoot, WebsocketServer::IsCompiled() ? validRoot : "");

    // It should handle an invalid path
    vm.clear();
    options.m_websocketServerProgramOptions.m_guiDocumentRoot = "";
    vm.insert(std::make_pair("document-root", boost::program_options::variable_value("foobar", false)));
    BOOST_REQUIRE_EQUAL(options.ParseFromVariableMap(vm), (WebsocketServer::IsCompiled() ? false : true) );
    BOOST_REQUIRE_EQUAL("", options.m_websocketServerProgramOptions.m_guiDocumentRoot);

    // It should handle an invalid value
    vm.clear();
    options.m_websocketServerProgramOptions.m_guiDocumentRoot = "";
    vm.insert(std::make_pair("document-root", boost::program_options::variable_value(100, false)));
    BOOST_REQUIRE_EQUAL(options.ParseFromVariableMap(vm), (WebsocketServer::IsCompiled() ? false : true));
    BOOST_REQUIRE_EQUAL("", options.m_websocketServerProgramOptions.m_guiDocumentRoot);
}

BOOST_AUTO_TEST_CASE(TelemetryRunnerProgramOptionsPortNumberTestCase)
{
    boost::program_options::variables_map vm;
    TelemetryRunnerProgramOptions options;

    // It should handle a valid integer
    vm.insert(std::make_pair("port-number", boost::program_options::variable_value(uint16_t(8000), false)));
    BOOST_REQUIRE(!options.ParseFromVariableMap(vm)); //fails because document root is missing but still sets port
    BOOST_REQUIRE_EQUAL(options.m_websocketServerProgramOptions.m_guiPortNumber, WebsocketServer::IsCompiled() ? "8000" : "");

    // It should handle an invalid value
    vm.clear();
    options.m_websocketServerProgramOptions.m_guiPortNumber = "";
    vm.insert(std::make_pair("port-number", boost::program_options::variable_value("foobar", false)));
    BOOST_REQUIRE_EQUAL(options.ParseFromVariableMap(vm), (WebsocketServer::IsCompiled() ? false : true));
    BOOST_REQUIRE_EQUAL("", options.m_websocketServerProgramOptions.m_guiPortNumber);
}
