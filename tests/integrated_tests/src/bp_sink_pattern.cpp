#include <BinaryConversions.h>
#include <Environment.h>
#include <InductsConfig.h>
#include <app_patterns/BpSinkPattern.h>
#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/mutex.hpp>
#include <iostream>

class TestSink : public BpSinkPattern {
public:
    bool ProcessPayload(const uint8_t *data, const uint64_t size) override;
    std::vector<std::vector<uint8_t>> m_payloads;
    boost::mutex m_mutex;
    boost::condition_variable m_cv;
};

bool TestSink::ProcessPayload(const uint8_t *data, const uint64_t size) {
    boost::mutex::scoped_lock lock(m_mutex);

    m_payloads.emplace_back(size);
    std::vector<uint8_t> &v = m_payloads.back();
    memcpy(v.data(), data, size);

    m_cv.notify_all();

    return true;
}

static void SendUdpPacket(const std::string host, int port, void *data, size_t size) {
    boost::asio::io_service ios;
    boost::asio::ip::udp::resolver resolver(ios);
    boost::asio::ip::udp::resolver::query query(
            boost::asio::ip::udp::v4(), host, std::to_string(port));

    boost::asio::ip::udp::resolver::iterator it = resolver.resolve(query);
    bool resolved_endpoint = (it != boost::asio::ip::udp::resolver::iterator());
    BOOST_REQUIRE(resolved_endpoint);
    if(!resolved_endpoint) {
        return;
    }

    boost::asio::ip::udp::endpoint ep = *it;

    boost::asio::ip::udp::socket socket(ios);
    socket.open(boost::asio::ip::udp::v4());
    boost::asio::const_buffer b(data, size);
    socket.send_to(b, ep);
}

static InductsConfig_ptr GetInductsConfig(std::string configName) {
    boost::filesystem::path p = 
        Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / configName;
    return InductsConfig::CreateFromJsonFilePath(p);
}

BOOST_AUTO_TEST_CASE(it_TestBpSinkPatternFragment, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestBpSinkPatternFragment" << std::endl << std::flush;

    TestSink sink;

    InductsConfig_ptr inducts = GetInductsConfig("bpsink_one_udp_port4557.json");
    BOOST_REQUIRE(inducts != NULL);

    OutductsConfig_ptr outducts; // null

    cbhe_eid_t eid(149, 1); // From bundles below
    sink.Init(inducts, outducts, boost::filesystem::path(), false, eid, 25, 1000);

    // TODO better way? Need to wait for UDP induct to start listening
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    // Check initial condition
    {
        boost::mutex::scoped_lock lock(sink.m_mutex);
        BOOST_REQUIRE(sink.m_payloads.size() == 0);
    }

    // Send bundle(s)
    const std::string expectedPayload("abcdefghijklmnopqrstuvwxyz\n");
    const std::string fragAHex(
            "06811116811501811501811501000082"
            "e3c9823b01822c00001b05110a69706e"
            "003134392e3000140101000109146162"
            "636465666768696a6b6c6d6e6f707172"
            "7374"
            );
    const std::string fragBHex(
            "06811116811501811501811501000082"
            "e3c9823b01822c00141b05110a69706e"
            "003134392e3000140101000109077576"
            "7778797a0a"
            );

    std::vector<uint8_t> fragA, fragB;

    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(fragAHex, fragA));
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(fragBHex, fragB));

    SendUdpPacket("127.0.0.1", 4557, fragA.data(), fragA.size());
    SendUdpPacket("127.0.0.1", 4557, fragB.data(), fragB.size());

    // Wait for data and validate
    {
        boost::mutex::scoped_lock lock(sink.m_mutex);
        if(sink.m_payloads.size() == 0) {
            // Not received yet, so wait for it
            boost::posix_time::ptime timeout(
                    boost::posix_time::microsec_clock::universal_time()
                    + boost::posix_time::milliseconds(100));
            bool notified = sink.m_cv.timed_wait(lock, timeout);
            BOOST_REQUIRE(notified);
        }

        BOOST_REQUIRE(sink.m_payloads.size() == 1);

        std::vector<uint8_t> p = sink.m_payloads[0];

        BOOST_REQUIRE(p.size() == expectedPayload.size());
        BOOST_REQUIRE(0 == memcmp(p.data(), expectedPayload.data(), expectedPayload.size()));
    }

    sink.Stop();
}
