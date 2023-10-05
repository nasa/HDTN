#include <boost/test/unit_test.hpp>
#include <boost/make_unique.hpp>
#include "TelemetryServer.h"
#include "TelemetryDefinitions.h"

class SocketMock {
    public:
        SocketMock() {
            context = zmq::context_t();
            server = boost::make_unique<zmq::socket_t>(context, zmq::socket_type::pair);
            server->bind(std::string("inproc://unit-test-sock"));
            
            client = boost::make_unique<zmq::socket_t>(context, zmq::socket_type::pair);
            client->connect(std::string("inproc://unit-test-sock"));\

            connId = ZmqConnectionId_t(5);
        }

        ~SocketMock() {
            server->close();
            client->close();
        }

        void QueueRequest(bool more) {
            ZmqConnectionId_t id(5);
            GetStorageApiCommand_t cmd;
            std::string cmdStr = cmd.ToJson();
            zmq::message_t msg(cmdStr.data(), cmdStr.size());
            zmq::message_t connId = id.Msg();

            client->send(std::move(connId), zmq::send_flags::sndmore);
            client->send(std::move(msg), more ? zmq::send_flags::sndmore : zmq::send_flags::none);
        }

        void QueueCorruptRequest(bool more) {
            std::string cmdStr = "junk";
            zmq::message_t msg(cmdStr.data(), cmdStr.size());
            zmq::message_t id = connId.Msg();

            client->send(std::move(id), zmq::send_flags::sndmore);
            client->send(std::move(msg), more ? zmq::send_flags::sndmore : zmq::send_flags::none);
        }

        void QueueOnlyConnectionID() {
            zmq::message_t id = connId.Msg();
            client->send(std::move(id), zmq::send_flags::sndmore);
        }

        zmq::message_t Receive() {
            zmq::message_t msg;
            const zmq::recv_result_t res = client->recv(msg, zmq::recv_flags::dontwait);
            return msg;
        }

        std::unique_ptr<zmq::socket_t> server;
        std::unique_ptr<zmq::socket_t> client;
        zmq::context_t context;
        ZmqConnectionId_t connId;
};

BOOST_AUTO_TEST_CASE(TelemetryServerConstructTestCase) {
    TelemetryServer server;
}

BOOST_AUTO_TEST_CASE(TelemetryServerReadRequest) {
    TelemetryServer server;
    SocketMock mock;

    // Test receiving one request
    mock.QueueRequest(false);
    TelemetryRequest request = server.ReadRequest(mock.server);
    BOOST_REQUIRE_EQUAL(request.Error(), false);
    BOOST_REQUIRE_EQUAL(request.More(), false);
    GetStorageApiCommand_t cmd;
    std::string cmdStr = cmd.ToJson();
    BOOST_REQUIRE_EQUAL(request.Command()->ToJson(), cmdStr);

    // Test receving multiple requests
    mock.QueueRequest(true);
    mock.QueueRequest(false);
    request = server.ReadRequest(mock.server);
    BOOST_REQUIRE_EQUAL(request.Error(), false);
    BOOST_REQUIRE_EQUAL(request.More(), true);
    request = server.ReadRequest(mock.server);
    BOOST_REQUIRE_EQUAL(request.Error(), false);
    BOOST_REQUIRE_EQUAL(request.More(), false);

    // Test error (no connection ID)
    request = server.ReadRequest(mock.server);
    BOOST_REQUIRE_EQUAL(request.Error(), true);

    // Test error (no message body)
    mock.QueueOnlyConnectionID();
    request = server.ReadRequest(mock.server);
    BOOST_REQUIRE_EQUAL(request.Error(), true);

    // Test error (corrupt payload)
    mock.QueueCorruptRequest(false);
    request = server.ReadRequest(mock.server);
    BOOST_REQUIRE_EQUAL(request.Error(), true);
}

BOOST_AUTO_TEST_CASE(TelemetryServerSendResponse) {
    TelemetryServer server;
    SocketMock mock;
    
    mock.QueueRequest(false);
    TelemetryRequest request = server.ReadRequest(mock.server);
    std::string resp = "HDTN-TEST";
    request.SendResponse(resp, mock.server);
    zmq::message_t connId = mock.Receive();
    zmq::message_t apiCommand = mock.Receive();
    zmq::message_t body = mock.Receive();
    BOOST_REQUIRE(mock.connId == connId);
    BOOST_REQUIRE_EQUAL(apiCommand.to_string(), "get_storage");
    BOOST_REQUIRE_EQUAL(body.to_string(), "HDTN-TEST");

    mock.QueueRequest(false);
    request = server.ReadRequest(mock.server);
    request.SendResponseSuccess(mock.server);
    connId = mock.Receive();
    apiCommand = mock.Receive();
    body = mock.Receive();
    BOOST_REQUIRE(mock.connId == connId);
    BOOST_REQUIRE_EQUAL(apiCommand.to_string(), "get_storage");
    ApiResp_t response;
    response.m_success = true;
    BOOST_REQUIRE_EQUAL(body.to_string(), response.ToJson());

    mock.QueueRequest(false);
    request = server.ReadRequest(mock.server);
    std::string error = "some error";
    request.SendResponseError(error, mock.server);
    connId = mock.Receive();
    apiCommand = mock.Receive();
    body = mock.Receive();
    BOOST_REQUIRE(mock.connId == connId);
    BOOST_REQUIRE_EQUAL(apiCommand.to_string(), "get_storage");
    ApiResp_t response2;
    response2.m_success = false;
    response2.m_message = "some error";
    BOOST_REQUIRE_EQUAL(body.to_string(), response2.ToJson());
}