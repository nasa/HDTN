#include <boost/make_unique.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>

#include "TelemetryConnection.h"
#include "Telemetry.h"

class MockTelemetrySender
{
public:
    std::unique_ptr<zmq::socket_t> m_respSocket;
    zmq::pollitem_t m_pollItems[1];

    MockTelemetrySender(std::string addr, zmq::context_t *inprocContextPtr)
    {
        m_respSocket = boost::make_unique<zmq::socket_t>(*inprocContextPtr, zmq::socket_type::pair);
        m_respSocket->bind(addr);
        m_pollItems[0] = {m_respSocket->handle(), 0, ZMQ_POLLIN, 0};
    }

    ~MockTelemetrySender()
    {
        m_respSocket.reset();
    }

    void Send(uint8_t byte)
    {
        zmq::message_t msg(&byte, sizeof(byte));
        m_respSocket->send(std::move(msg), zmq::send_flags::dontwait);
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

BOOST_AUTO_TEST_CASE(TelemetryConnectionSendMessageTestCase)
{
    std::unique_ptr<zmq::context_t> contextPtr = boost::make_unique<zmq::context_t>(0);
    std::unique_ptr<MockTelemetrySender> sender = boost::make_unique<MockTelemetrySender>(
        "inproc://my-connection",
        contextPtr.get()
    );

    std::unique_ptr<TelemetryConnection> receiver = boost::make_unique<TelemetryConnection>("inproc://my-connection", contextPtr.get());
    sender->Send(GUI_REQ_MSG);
    uint8_t data = receiver->ReadMessage<uint8_t>();
    receiver.reset();
    sender.reset();
    BOOST_REQUIRE_EQUAL(GUI_REQ_MSG, data);
}

BOOST_AUTO_TEST_CASE(TelemetryConnectionHandleCase)
{
    std::unique_ptr<TelemetryConnection>connection = boost::make_unique<TelemetryConnection>("tcp://localhost:10301", nullptr);
    BOOST_TEST(connection->GetSocketHandle());
    connection.reset();
}
