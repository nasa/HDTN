/**
 * @file TestHdtnCliRunner.cpp
 * @author Ethan Schweinsberg <ethan.e.schweinsberg@gmail.com>
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
#include <zmq.hpp>
#include <boost/filesystem/fstream.hpp>
#include "HdtnCliRunner.h"
#include "TelemetryDefinitions.h"
#include "Environment.h"

static const std::string DEFAULT_HOSTNAME = "127.0.0.1";
static const std::string DEFAULT_PORT = "10305";

BOOST_AUTO_TEST_CASE(HdtnCliRunnerTestParseHostnameAndPort) {
    // Test defaults
    {
        const char* argv[] = { "HdtnCliRunnerTest", nullptr };
        std::string hostname;
        std::string port;
        HdtnCliRunner runner;
        bool success = runner.ParseHostnameAndPort(1, argv, hostname, port);
        BOOST_REQUIRE(success);
        BOOST_REQUIRE_EQUAL(hostname, DEFAULT_HOSTNAME);
        BOOST_REQUIRE_EQUAL(port, DEFAULT_PORT);
    }

    // Test hostname and port
    {
        const char* argv[] = { "HdtnCliRunnerTest", "--hostname=myhost", "--port=5000", nullptr };
        std::string hostname;
        std::string port;
        HdtnCliRunner runner;
        bool success = runner.ParseHostnameAndPort(3, argv, hostname, port);
        BOOST_REQUIRE(success);
        BOOST_REQUIRE_EQUAL(hostname, "myhost");
        BOOST_REQUIRE_EQUAL(port, "5000");
    }
}

BOOST_AUTO_TEST_CASE(HdtnCliRunnerTestConnectToHdtn) {
    // Test connect to offline HDTN
    {
        std::string hostname = "127.0.0.1";
        std::string port = "10305";
        HdtnCliRunner runner;
        bool success = runner.ConnectToHdtn(hostname, port);
        BOOST_REQUIRE(success);
    }

    // Test connect to online HDTN
    {
        // Create fake HDTN socket
        zmq::context_t context(1);
        zmq::socket_t socket(context, ZMQ_REP);
        socket.bind("tcp://*:10305");

        // Connect
        std::string hostname = "127.0.0.1";
        std::string port = "10305";
        HdtnCliRunner runner;
        bool success = runner.ConnectToHdtn(hostname, port);
        BOOST_REQUIRE(success);

        // Close socket
        socket.close();
    }
}

void HdtnMock(zmq::context_t& context, std::string& reply, std::vector<std::string> expectedRequests) {
    zmq::socket_t socket(context, ZMQ_REP);
    socket.bind("tcp://*:10305");

    while (true) {
        zmq::message_t request;

        // Wait for next request from client
        zmq::recv_result_t result = socket.recv(request, zmq::recv_flags::none);
        BOOST_REQUIRE_EQUAL(request.to_string(), expectedRequests[0]);
        expectedRequests.erase(expectedRequests.begin());

        // Send reply back to client
        zmq::message_t msg(reply);
        socket.send(msg, zmq::send_flags::none);

        if (expectedRequests.size() == 0) {
            break;
        }
    }
    socket.close();
}

BOOST_AUTO_TEST_CASE(HdtnCliRunnerTestSendRequest) {
    // Test send request to offline HDTN
    {
        std::string hostname = "127.0.0.1";
        std::string port = "10305";
        HdtnCliRunner runner;
        bool success = runner.ConnectToHdtn(hostname, port);
        BOOST_REQUIRE(success);

        std::string request = "test";
        std::string reply = runner.SendRequestToHdtn(request);
        BOOST_REQUIRE_EQUAL(reply, "");
    }

    // Test send request to online HDTN
    {
        // Start the HDTN mock
        zmq::context_t context(1);
        std::string reply = "hello client";
        boost::thread t(boost::bind(HdtnMock, boost::ref(context), reply, std::vector<std::string>{"hello server"}));

        // Connect
        std::string hostname = "127.0.0.1";
        std::string port = "10305";
        HdtnCliRunner runner;
        bool success = runner.ConnectToHdtn(hostname, port);
        BOOST_REQUIRE(success);

        // Send request
        std::string request = "hello server";
        std::string response = runner.SendRequestToHdtn(request);
        BOOST_REQUIRE_EQUAL(response, "hello client");
    }
}

BOOST_AUTO_TEST_CASE(HdtnCliRunnerTestGetHdtnConfig) {
    // Test get config from offline HDTN
    {
        // Start the HDTN mock with an HDTN config as the reply
        zmq::context_t context(1);
        boost::filesystem::path path = Environment::GetPathHdtnSourceRoot() / "module/cli/test/hdtn_config.json";
        boost::filesystem::ifstream file(path);
        std::string reply((std::istreambuf_iterator<char>(file)),
                                (std::istreambuf_iterator<char>()));
        file.close();
        GetHdtnConfigApiCommand_t cmd;
        boost::thread t(boost::bind(HdtnMock, boost::ref(context), reply, std::vector<std::string>{cmd.ToJson()}));

        std::string hostname = "127.0.0.1";
        std::string port = "10305";
        HdtnCliRunner runner;
        bool success = runner.ConnectToHdtn(hostname, port);
        BOOST_REQUIRE(success);

        HdtnConfig_ptr config = runner.GetHdtnConfig();
        BOOST_REQUIRE_NE(config, nullptr);
        t.join();
    }
}

BOOST_AUTO_TEST_CASE(HdtnCliRunnerTestParseCliOptions) {
    // Test parse options success
    {
        // First, get the HDTN config
        boost::filesystem::path path = Environment::GetPathHdtnSourceRoot() / "module/cli/test/hdtn_config.json";
        HdtnConfig_ptr config = HdtnConfig::CreateFromJsonFilePath(path);

        // Now parse the options
        const char* argv[] = { "HdtnCliRunnerTest", "--hostname=myhost", "--port=5000",
            "--help", "--contact-plan-file=my-file", "--contact-plan-json={}", "--outduct[0].rateBps=1000", nullptr };
        boost::program_options::variables_map vm;
        HdtnCliRunner runner;
        bool success = runner.ParseCliOptions(7, argv, config, vm);
        BOOST_REQUIRE(success);
        BOOST_REQUIRE(vm.count("help"));
        BOOST_REQUIRE_EQUAL(vm["hostname"].as<std::string>(), "myhost");
        BOOST_REQUIRE_EQUAL(vm["port"].as<std::string>(), "5000");
        BOOST_REQUIRE_EQUAL(vm["contact-plan-file"].as<std::string>(), "my-file");
        BOOST_REQUIRE_EQUAL(vm["contact-plan-json"].as<std::string>(), "{}");
        BOOST_REQUIRE_EQUAL(vm["outduct[0].rateBps"].as<uint64_t>(), 1000);
    }

    // Test parse options bad option
    {
         // First, get the HDTN config
        boost::filesystem::path path = Environment::GetPathHdtnSourceRoot() / "module/cli/test/hdtn_config.json";
        HdtnConfig_ptr config = HdtnConfig::CreateFromJsonFilePath(path);

        // Now parse the options
        const char* argv[] = { "HdtnCliRunnerTest", "--bad-option", nullptr };
        boost::program_options::variables_map vm;
        HdtnCliRunner runner;
        bool success = runner.ParseCliOptions(2, argv, config, vm);
        BOOST_REQUIRE(!success);
    }

    // Test parse options invalid outduct
    {
        // First, get the HDTN config
        boost::filesystem::path path = Environment::GetPathHdtnSourceRoot() / "module/cli/test/hdtn_config.json";
        HdtnConfig_ptr config = HdtnConfig::CreateFromJsonFilePath(path);

        // Now parse the options
        const char* argv[] = { "HdtnCliRunnerTest", "--outducts[1].rateBps=1000", nullptr };
        boost::program_options::variables_map vm;
        HdtnCliRunner runner;
        bool success = runner.ParseCliOptions(2, argv, config, vm);
        BOOST_REQUIRE(!success);
    }
}


BOOST_AUTO_TEST_CASE(HdtnCliRunnerTestExecuteCliOptions) {
    // Test execute options success
    {
        // First, get the HDTN config
        boost::filesystem::path path = Environment::GetPathHdtnSourceRoot() / "module/cli/test/hdtn_config.json";
        HdtnConfig_ptr config = HdtnConfig::CreateFromJsonFilePath(path);

        // Get the contact plan
        boost::filesystem::path contactPlanPath = Environment::GetPathHdtnSourceRoot() / "module/cli/test/contact_plan.json";
        std::string contactPlanJson;
        boost::filesystem::ifstream file(contactPlanPath);
        contactPlanJson = std::string((std::istreambuf_iterator<char>(file)),
                                (std::istreambuf_iterator<char>()));
        file.close();

        // Connect to HDTN
        std::string hostname = "127.0.0.1";
        std::string port = "10305";
         HdtnCliRunner runner;
        bool success = runner.ConnectToHdtn(hostname, port);
        BOOST_REQUIRE(success);

        // Now parse the options
        std::string contactPlanJsonOption = std::string("--contact-plan-json=") + contactPlanJson;
        std::string contactPlanFileOption = std::string("--contact-plan-file=") + contactPlanPath.string();
        const char* argv[] = { "HdtnCliRunnerTest", "--hostname=myhost", "--port=5000",
            "--help", contactPlanFileOption.c_str(),
            contactPlanJsonOption.c_str(),
             "--outduct[0].rateBps=1000", nullptr };

        boost::program_options::variables_map vm;
        success = runner.ParseCliOptions(7, argv, config, vm);
        BOOST_REQUIRE(success);

        zmq::context_t context(1);
        std::string reply = "success";
        std::vector<std::string> expectedRequests;

        // Build all of the expected commands
        // Upload contact plan command #1
        UploadContactPlanApiCommand_t uploadContactPlanCmd;
        uploadContactPlanCmd.m_contactPlanJson = contactPlanJson;
        expectedRequests.push_back(uploadContactPlanCmd.ToJson());

        // Upload contact plan command #2
        expectedRequests.push_back(uploadContactPlanCmd.ToJson());

        // Set outduct rate command
        SetMaxSendRateApiCommand_t setMaxSendRateCmd;
        setMaxSendRateCmd.m_outduct = 0;
        setMaxSendRateCmd.m_rateBitsPerSec = 1000;
        expectedRequests.push_back(setMaxSendRateCmd.ToJson());

        // Now execute the options
        boost::thread t(boost::bind(HdtnMock, boost::ref(context), reply, expectedRequests));
        success = runner.ExecuteCliOptions(vm);
        BOOST_REQUIRE(success);
        t.join();
    }
}
