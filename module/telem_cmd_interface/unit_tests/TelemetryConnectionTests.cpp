#include <boost/make_unique.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>

#include "TelemetryConnection.h"
#include "TelemetryDefinitions.h"

class MockTelemetryResponder
{
public:
    std::unique_ptr<zmq::socket_t> m_respSocket;
    zmq::pollitem_t m_pollItems[1];

    MockTelemetryResponder(std::string addr, zmq::context_t *inprocContextPtr)
    {
        m_respSocket = boost::make_unique<zmq::socket_t>(*inprocContextPtr, zmq::socket_type::pair);
        m_respSocket->bind(addr);
        m_pollItems[0] = {m_respSocket->handle(), 0, ZMQ_POLLIN, 0};
    }

    ~MockTelemetryResponder()
    {
        m_respSocket.reset();
    }

    void Send(uint8_t byte)
    {
        zmq::message_t msg(&byte, sizeof(byte));
        m_respSocket->send(std::move(msg), zmq::send_flags::dontwait);
    }

    uint8_t Read()
    {
        uint8_t data = 0;
        const zmq::recv_buffer_result_t res = m_respSocket->recv(zmq::mutable_buffer(&data, sizeof(data)), zmq::recv_flags::dontwait);
        return data;
    }
};

BOOST_AUTO_TEST_CASE(TelemetryConnectionInitTestCase)
{
    std::unique_ptr<TelemetryConnection> connection;
    // Attempt a valid TCP connection
    BOOST_REQUIRE_NO_THROW(connection = boost::make_unique<TelemetryConnection>("tcp://localhost:10301", nullptr));
    connection.reset();

    // Attempt a valid inproc connection
    std::unique_ptr<zmq::context_t> contextPtr = boost::make_unique<zmq::context_t>(0);
    BOOST_REQUIRE_NO_THROW(connection = boost::make_unique<TelemetryConnection>("inproc://my-connection", contextPtr.get()));
    connection.reset();

    // Attempt an invalid connection
    BOOST_REQUIRE_THROW(connection = boost::make_unique<TelemetryConnection>("tcp://invalid-addr", nullptr), zmq::error_t);
    connection.reset();
}

BOOST_AUTO_TEST_CASE(TelemetryConnectionReadMessageTestCase)
{
    std::unique_ptr<zmq::context_t> contextPtr = boost::make_unique<zmq::context_t>(0);
    std::unique_ptr<MockTelemetryResponder> responder = boost::make_unique<MockTelemetryResponder>(
        "inproc://my-connection",
        contextPtr.get()
    );

    std::unique_ptr<TelemetryConnection> requester = boost::make_unique<TelemetryConnection>("inproc://my-connection", contextPtr.get());
    responder->Send(TELEM_REQ_MSG);
    zmq::message_t msg = requester->ReadMessage();
    requester.reset();
    responder.reset();
    BOOST_REQUIRE_EQUAL(msg.size(), 1);
    BOOST_REQUIRE_EQUAL(TELEM_REQ_MSG, *((uint8_t*)(msg.data())));
}

BOOST_AUTO_TEST_CASE(TelemetryConnectionSendMessageTestCase)
{
    std::unique_ptr<zmq::context_t> contextPtr = boost::make_unique<zmq::context_t>(0);
    std::unique_ptr<MockTelemetryResponder> responder = boost::make_unique<MockTelemetryResponder>(
        "inproc://my-connection",
        contextPtr.get()
    );

    std::unique_ptr<TelemetryConnection> requester = boost::make_unique<TelemetryConnection>("inproc://my-connection", contextPtr.get());
    uint8_t sendData = 0x05;
    static const zmq::const_buffer buf(&sendData, sizeof(sendData));
    requester->SendMessage(buf);
    uint8_t receiveData = responder->Read();
    BOOST_REQUIRE_EQUAL(sendData, receiveData);
}

BOOST_AUTO_TEST_CASE(TelemetryConnectionGetSocketHandleTestCase)
{
    std::unique_ptr<TelemetryConnection>connection = boost::make_unique<TelemetryConnection>("tcp://localhost:10301", nullptr);
    BOOST_TEST(connection->GetSocketHandle());
    connection.reset();
}
