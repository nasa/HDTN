#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>

#include "TelemetryConnectionPoller.h"
#include "TelemetryConnection.h"

BOOST_AUTO_TEST_CASE(TelemetryConnectionPollerAddConnectionTestCase)
{
    TelemetryConnection connection("tcp://localhost:10301", nullptr, zmq::socket_type::req);
    TelemetryConnectionPoller poller;

    // A connection should get added successfully
    poller.AddConnection(connection);
    BOOST_REQUIRE_EQUAL(1, poller.m_pollItems.size());
}

BOOST_AUTO_TEST_CASE(TelemetryConnectionPollerPollConnectionsTestCase)
{
    TelemetryConnectionPoller poller;
    TelemetryConnection connection("tcp://localhost:10301", nullptr, zmq::socket_type::req);
    poller.AddConnection(connection);

    TelemetryConnection connection2("tcp://localhost:10302", nullptr, zmq::socket_type::req);
    poller.AddConnection(connection2);

    // Set revents to ensure it gets cleared
    poller.m_pollItems[0].revents |= ZMQ_POLLIN;
    poller.m_pollItems[1].revents |= ZMQ_POLLIN;
    boost::timer::cpu_timer timer;
    poller.PollConnections(100);
    BOOST_REQUIRE_CLOSE(double(timer.elapsed().wall), 100 * 1000000.0, 50);
    BOOST_REQUIRE_EQUAL(0, poller.m_pollItems[0].revents);
    BOOST_REQUIRE_EQUAL(0, poller.m_pollItems[1].revents);
}

BOOST_AUTO_TEST_CASE(TelemetryConnectionPollerHasNewMessageTestCase)
{
    TelemetryConnectionPoller poller;
    TelemetryConnection addedConnection("tcp://localhost:10301", nullptr, zmq::socket_type::req);
    poller.AddConnection(addedConnection);

    // An un-added connection should not have a new message
    TelemetryConnection unaddedConnection("tcp://localhost:10302", nullptr, zmq::socket_type::req);
    BOOST_REQUIRE_EQUAL(false, poller.HasNewMessage(unaddedConnection));
 
    // An added connection with no event shouldn't have a new message
    BOOST_REQUIRE_EQUAL(false, poller.HasNewMessage(addedConnection));

    // An added connection with an revent should have a new message
    poller.m_pollItems[0].revents |= ZMQ_POLLIN;
    BOOST_REQUIRE_EQUAL(true, poller.HasNewMessage(addedConnection));

    // A second connection should also work
    TelemetryConnection addedConnection2("tcp://localhost:10302", nullptr, zmq::socket_type::req);;
    poller.AddConnection(addedConnection2);
    BOOST_REQUIRE_EQUAL(false, poller.HasNewMessage(addedConnection2));
    poller.m_pollItems[1].revents |= ZMQ_POLLIN;
    BOOST_REQUIRE_EQUAL(true, poller.HasNewMessage(addedConnection2));
}
