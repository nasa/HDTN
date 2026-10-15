// SPDX-License-Identifier: Apache-2.0

#include <boost/test/unit_test.hpp>
#include "TcpclV3BidirectionalLink.h"
#include "TcpclBundleSource.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace {

std::vector<uint8_t> Payload(unsigned int id, std::size_t size = 23) {
    std::vector<uint8_t> result(size);
    for (std::size_t i = 0; i < size; ++i) {
        result[i] = static_cast<uint8_t>(id * 13 + i);
    }
    return result;
}

// Drives the real link, sender and TCP socket on loopback. Polling keeps write
// completion and error timing under the test's control; the peer never sends an
// ACK unless the test explicitly supplies one to the receive state machine.
class CompletionLink : public TcpclV3BidirectionalLink {
public:
    explicit CompletionLink(uint64_t fragmentSize = 0) :
        TcpclV3BidirectionalLink("completion test", 0, false, true, 0, NULL,
            3, 16 * 1024 * 1024, fragmentSize, 1, "ipn:2.0"),
        peer(m_base_ioServiceRef), completed(0), failed(0), notifications(0) {
        BaseClass_SetUserAssignedUuid(73);
        BaseClass_SetOnSuccessfulBundleSendCallback([this](std::vector<uint8_t>& data, uint64_t uuid) {
            BOOST_CHECK_EQUAL(uuid, 73);
            successfulData.push_back(data);
            ++completed;
        });
        BaseClass_SetOnFailedBundleVecSendCallback([this](padded_vector_uint8_t& bundle,
            std::vector<uint8_t>& data, uint64_t uuid, bool successCalled) {
            BOOST_CHECK_EQUAL(uuid, 73);
            BOOST_CHECK(!successCalled);
            failedPayloads.emplace_back(bundle.begin(), bundle.end());
            failedData.push_back(data);
            ++failed;
        });
        BaseClass_SetOnFailedBundleZmqSendCallback([this](zmq::message_t& bundle,
            std::vector<uint8_t>& data, uint64_t uuid, bool successCalled) {
            BOOST_CHECK_EQUAL(uuid, 73);
            BOOST_CHECK(!successCalled);
            const uint8_t* bytes = static_cast<const uint8_t*>(bundle.data());
            failedPayloads.emplace_back(bytes, bytes + bundle.size());
            failedData.push_back(data);
            ++failed;
        });
        Connect();
    }

    ~CompletionLink() {
        boost::system::error_code ignored;
        peer.close(ignored);
        m_base_tcpSocketPtr->close(ignored);
        // Drain socket cancellation before destroying TcpAsyncSender.
        Poll();
        m_base_noKeepAlivePacketReceivedTimer.cancel();
        m_base_needToSendKeepAliveMessageTimer.cancel();
        m_base_sendShutdownMessageTimeoutTimer.cancel();
        Poll();
    }

    void Connect() {
        using boost::asio::ip::tcp;
        tcp::acceptor acceptor(m_base_ioServiceRef, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
        m_base_tcpSocketPtr = std::make_shared<tcp::socket>(m_base_ioServiceRef);
        peer = tcp::socket(m_base_ioServiceRef);
        m_base_tcpSocketPtr->connect(acceptor.local_endpoint());
        acceptor.accept(peer);
        peer.non_blocking(true);
        m_base_tcpAsyncSenderPtr.reset(new TcpAsyncSender(m_base_tcpSocketPtr, m_base_ioServiceRef));
        m_base_tcpAsyncSenderPtr->SetOnFailedBundleVecSendCallback(m_base_onFailedBundleVecSendCallback);
        m_base_tcpAsyncSenderPtr->SetOnFailedBundleZmqSendCallback(m_base_onFailedBundleZmqSendCallback);
        m_base_tcpAsyncSenderPtr->SetUserAssignedUuid(73);
        m_base_shutdownCalled = false;
        m_base_tcpclShutdownComplete = false;
    }

    void Negotiate(uint8_t flags) {
        // Independent RFC 7242 encoding: magic, version, flags, keepalive, EID.
        Input({'d', 't', 'n', '!', 3, flags, 0, 0, 7, 'i', 'p', 'n', ':', '2', '.', '0'});
        BOOST_REQUIRE(m_base_readyToForward.load());
    }

    void Input(const std::vector<uint8_t>& bytes) {
        m_base_tcpclV3RxStateMachine.HandleReceivedChars(bytes.data(), bytes.size());
    }

    bool Queue(unsigned int id, bool zmqData = false, std::size_t size = 23) {
        const std::vector<uint8_t> bytes = Payload(id, size);
        if (zmqData) {
            zmq::message_t bundle(bytes.data(), bytes.size());
            return BaseClass_Forward(bundle, {static_cast<uint8_t>(id)});
        }
        padded_vector_uint8_t bundle(bytes.begin(), bytes.end());
        return BaseClass_Forward(bundle, {static_cast<uint8_t>(id)});
    }

    void Keepalive() {
        TcpAsyncSenderElement* el = new TcpAsyncSenderElement;
        el->m_underlyingDataVecHeaders = {{0x40}};
        el->m_constBufferVec.push_back(boost::asio::buffer(el->m_underlyingDataVecHeaders[0]));
        el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
        m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
    }

    void Poll() {
        m_base_ioServiceRef.reset();
        m_base_ioServiceRef.poll();
    }

    void Read() {
        if (!peer.is_open()) return;
        uint8_t bytes[8192];
        boost::system::error_code error;
        for (;;) {
            const std::size_t n = peer.read_some(boost::asio::buffer(bytes), error);
            wire.insert(wire.end(), bytes, bytes + n);
            if (error == boost::asio::error::would_block || error == boost::asio::error::try_again) return;
            BOOST_REQUIRE_MESSAGE(!error, error.message());
        }
    }

    template <typename Predicate> bool Until(Predicate predicate, bool read = true) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        do {
            Poll();
            if (read) Read();
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }

    void ResetPeer() {
        peer.set_option(boost::asio::socket_base::linger(true, 0));
        peer.close();
    }

    void SmallBuffers() {
        m_base_tcpSocketPtr->set_option(boost::asio::socket_base::send_buffer_size(1024));
        peer.set_option(boost::asio::socket_base::receive_buffer_size(1024));
    }

    void FinishShutdown() {
        BOOST_REQUIRE(Until([this]() { return m_base_tcpclShutdownComplete.load(); }, false));
    }

    bool Empty() const { return m_base_bytesToAckCb.IsEmpty(); }
    bool Full() const { return m_base_bytesToAckCb.IsFull(); }
    uint64_t FailedCount() const { return m_base_telem.totalBundlesFailedToSend.load(); }
    uint64_t FragmentsAcked() const { return m_base_telem.totalFragmentsSentAndAcked.load(); }

    boost::asio::ip::tcp::socket peer;
    unsigned int completed, failed, notifications;
    std::vector<std::vector<uint8_t> > successfulData, failedData, failedPayloads;
    std::vector<uint8_t> wire;

private:
    void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread() override {}
    void Virtual_OnSuccessfulWholeBundleAcknowledged() override { ++notifications; }
    void Virtual_WholeBundleReady(padded_vector_uint8_t&) override {}
};

uint64_t ReadSdnv(const std::vector<uint8_t>& wire, std::size_t& offset) {
    uint64_t value = 0;
    unsigned int count = 0;
    uint8_t byte;
    do {
        BOOST_REQUIRE(offset < wire.size());
        BOOST_REQUIRE(++count <= 10);
        byte = wire[offset++];
        value = (value << 7) | (byte & 0x7f);
    } while (byte & 0x80);
    return value;
}

void CheckWire(const std::vector<uint8_t>& wire, unsigned int count, uint64_t fragmentSize, bool lengths) {
    const std::vector<uint8_t> contact = {'d', 't', 'n', '!', 3, 1, 0, 0, 7, 'i', 'p', 'n', ':', '1', '.', '0'};
    BOOST_REQUIRE(wire.size() >= contact.size());
    BOOST_CHECK_EQUAL_COLLECTIONS(contact.begin(), contact.end(), wire.begin(), wire.begin() + contact.size());
    std::size_t offset = contact.size();
    for (unsigned int id = 0; id < count; ++id) {
        while (offset < wire.size() && wire[offset] == 0x40) ++offset;
        if (lengths) {
            BOOST_REQUIRE(offset < wire.size());
            BOOST_CHECK_EQUAL(wire[offset++], 0x60);
            BOOST_CHECK_EQUAL(ReadSdnv(wire, offset), 23);
        }
        std::vector<uint8_t> bundle;
        do {
            BOOST_REQUIRE(offset < wire.size());
            const uint8_t header = wire[offset++];
            BOOST_CHECK_EQUAL(header & 0xf0, 0x10);
            BOOST_CHECK_EQUAL(bool(header & 2), bundle.empty());
            const uint64_t length = ReadSdnv(wire, offset);
            BOOST_CHECK_EQUAL(length, std::min<uint64_t>(fragmentSize ? fragmentSize : 23, 23 - bundle.size()));
            BOOST_REQUIRE(length <= wire.size() - offset);
            bundle.insert(bundle.end(), wire.begin() + offset, wire.begin() + offset + length);
            offset += length;
            BOOST_CHECK_EQUAL(bool(header & 1), bundle.size() == 23);
        } while (bundle.size() < 23);
        BOOST_CHECK(bundle == Payload(id));
    }
    while (offset < wire.size() && wire[offset] == 0x40) ++offset;
    BOOST_CHECK_EQUAL(offset, wire.size());
}

void CheckNoAckProgress(uint64_t fragmentSize, uint8_t flags, bool zmqData) {
    CompletionLink link(fragmentSize);
    link.Negotiate(flags);
    // The ring's seven usable slots must remain bounded and be reused for
    // three complete windows, independently of the outer pipeline accounting.
    for (unsigned int window = 0; window < 3; ++window) {
        for (unsigned int j = 0; j < 7; ++j) {
            BOOST_REQUIRE(link.Queue(window * 7 + j, zmqData));
        }
        BOOST_CHECK(link.Full());
        BOOST_CHECK(!link.Queue(99, zmqData));
        BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesUnacked(), 7);
        // An unexpected protocol ACK cannot retire no-ACK state.
        if (window != 0) link.Input({0x20, 23});
        BOOST_CHECK_EQUAL(link.completed, window * 7);
        link.Keepalive();
        BOOST_REQUIRE_MESSAGE(link.Until([&]() { return link.completed == (window + 1) * 7; }),
            "Completed " << link.completed << " of " << (window + 1) * 7 << " bundles");
        BOOST_CHECK(link.Empty());
        BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesUnacked(), 0);
        BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundleBytesUnacked(), 0);
        const BidirectionalLink& base = link;
        BOOST_CHECK_EQUAL(base.BaseClass_GetTotalBundlesUnacked(), 0);
        BOOST_CHECK_EQUAL(base.BaseClass_GetTotalBundleBytesUnacked(), 0);
    }
    BOOST_CHECK_EQUAL(link.notifications, 21);
    BOOST_CHECK_EQUAL(link.failed, 0);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesAcked(), 0);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundleBytesAcked(), 0);
    BOOST_CHECK_EQUAL(link.FragmentsAcked(), 0);
    for (unsigned int id = 0; id < 21; ++id) {
        BOOST_CHECK(link.successfulData[id] == std::vector<uint8_t>(1, static_cast<uint8_t>(id)));
    }
    // Local write completion may precede peer receipt. Wait for the independent
    // endpoint to receive all segments and the three explicit keepalives.
    const std::size_t segments = fragmentSize ? (23 + fragmentSize - 1) / fragmentSize : 1;
    const std::size_t expectedBytes = 16 + 21 * (23 + 2 * segments + ((flags & 8) ? 2 : 0)) + 3;
    BOOST_REQUIRE_MESSAGE(link.Until([&]() { return link.wire.size() >= expectedBytes; }),
        "Peer received " << link.wire.size() << " of " << expectedBytes << " bytes");
    CheckWire(link.wire, 21, fragmentSize, flags & 8);
}

void CheckFailure(bool zmqData, uint64_t fragmentSize) {
    CompletionLink link(fragmentSize);
    link.Negotiate(0);
    link.SmallBuffers();
    const std::size_t size = 4 * 1024 * 1024;
    BOOST_REQUIRE(link.Queue(1, zmqData, size));
    BOOST_REQUIRE(link.Queue(2, zmqData, size));
    BOOST_REQUIRE(link.Until([&]() { return link.peer.available() > 100; }, false));
    BOOST_CHECK_EQUAL(link.completed, 0); // No early-fragment completion.
    link.ResetPeer();
    BOOST_REQUIRE(link.Until([&]() { return link.failed == 2; }, false));
    BOOST_CHECK_EQUAL(link.completed, 0);
    BOOST_CHECK_EQUAL(link.notifications, 0);
    BOOST_CHECK_EQUAL(link.FailedCount(), 2);
    BOOST_CHECK(link.Empty());
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesUnacked(), 0);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundleBytesUnacked(), 0);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesAcked(), 0);
    for (unsigned int id = 1; id <= 2; ++id) {
        BOOST_CHECK(link.failedData[id - 1] == std::vector<uint8_t>(1, id));
        BOOST_CHECK(link.failedPayloads[id - 1] == Payload(id, size));
    }
    link.FinishShutdown();
    // Reconnect with ACKs enabled. Old local completions cannot release this
    // connection's bundle or inflate its protocol acknowledgment counters.
    link.Connect();
    link.Negotiate(1);
    link.wire.clear();
    BOOST_REQUIRE(link.Queue(3));
    BOOST_REQUIRE(link.Until([&]() { return link.wire.size() == 41; }));
    BOOST_CHECK_EQUAL(link.completed, 0);
    link.Input({0x20, 23});
    BOOST_REQUIRE_EQUAL(link.completed, 1);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesUnacked(), 0);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesAcked(), 1);
    BOOST_CHECK(link.successfulData.back() == std::vector<uint8_t>(1, 3));
}

} // namespace

BOOST_AUTO_TEST_CASE(TcpclV3NoAckVectorCompletion) { CheckNoAckProgress(0, 0, false); }
BOOST_AUTO_TEST_CASE(TcpclV3NoAckZmqCompletion) { CheckNoAckProgress(0, 8, true); }
BOOST_AUTO_TEST_CASE(TcpclV3NoAckFragmentCompletion) { CheckNoAckProgress(5, 8, false); }
BOOST_AUTO_TEST_CASE(TcpclV3NoAckZmqFragmentCompletion) { CheckNoAckProgress(5, 0, true); }
BOOST_AUTO_TEST_CASE(TcpclV3NoAckVectorFailure) { CheckFailure(false, 0); }
BOOST_AUTO_TEST_CASE(TcpclV3NoAckZmqFailure) { CheckFailure(true, 0); }
BOOST_AUTO_TEST_CASE(TcpclV3NoAckFragmentFailure) { CheckFailure(false, 65536); }
BOOST_AUTO_TEST_CASE(TcpclV3NoAckZmqFragmentFailure) { CheckFailure(true, 65536); }

BOOST_AUTO_TEST_CASE(TcpclV3AckCompletionUnchanged) {
    CompletionLink link(5);
    link.Negotiate(1);
    BOOST_REQUIRE(link.Queue(1));
    BOOST_REQUIRE(link.Until([&]() { return link.wire.size() >= 49; }));
    BOOST_CHECK_EQUAL(link.completed, 0);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesUnacked(), 1);
    for (uint8_t n : {5, 10, 15, 20}) {
        link.Input({0x20, n});
        BOOST_CHECK_EQUAL(link.completed, 0);
    }
    link.Input({0x20, 23});
    BOOST_CHECK_EQUAL(link.completed, 1);
    BOOST_CHECK_EQUAL(link.FragmentsAcked(), 5);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesAcked(), 1);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundleBytesAcked(), 23);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesUnacked(), 0);
}

BOOST_AUTO_TEST_CASE(TcpclV3NoAckEmptyPayloadCompletion) {
    CompletionLink link;
    link.Negotiate(0);
    padded_vector_uint8_t empty;
    BOOST_REQUIRE(link.BaseClass_Forward(empty, {42}));
    BOOST_REQUIRE(link.Until([&]() { return link.completed == 1; }));
    BOOST_CHECK(link.Empty());
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesSent(), 1);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesUnacked(), 0);
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundleBytesUnacked(), 0);
}

BOOST_AUTO_TEST_CASE(TcpclV3NoAckEmptyUserData) {
    CompletionLink link;
    link.Negotiate(0);
    const std::vector<uint8_t> bytes = Payload(0);
    padded_vector_uint8_t bundle(bytes.begin(), bytes.end());
    BOOST_REQUIRE(link.BaseClass_Forward(bundle, {}));
    BOOST_REQUIRE(link.Until([&]() { return link.completed == 1; }));
    BOOST_CHECK(link.successfulData.front().empty());
    BOOST_CHECK_EQUAL(link.notifications, 1);
    BOOST_CHECK(link.Empty());
    BOOST_CHECK_EQUAL(link.BaseClass_GetTotalBundlesUnacked(), 0);
}

BOOST_AUTO_TEST_CASE(TcpclV3NoAckZmqPayloadReleasedOnce) {
    for (bool fail : {false, true}) {
        std::atomic<unsigned int> released(0);
        CompletionLink link(fail ? 65536 : 5);
        link.Negotiate(0);
        if (fail) link.SmallBuffers();
        const std::vector<uint8_t> bytes = Payload(1, fail ? 4 * 1024 * 1024 : 23);
        uint8_t* owned = new uint8_t[bytes.size()];
        std::copy(bytes.begin(), bytes.end(), owned);
        zmq::message_t bundle(owned, bytes.size(), [](void* data, void* hint) {
            delete[] static_cast<uint8_t*>(data);
            static_cast<std::atomic<unsigned int>*>(hint)->fetch_add(1);
        }, &released);
        BOOST_REQUIRE(link.BaseClass_Forward(bundle, {1}));
        BOOST_CHECK_EQUAL(released.load(), 0);
        if (fail) {
            BOOST_REQUIRE(link.Until([&]() { return link.peer.available() > 100; }, false));
            BOOST_CHECK_EQUAL(released.load(), 0);
            link.ResetPeer();
            BOOST_REQUIRE(link.Until([&]() { return link.failed == 1; }, false));
            BOOST_CHECK_EQUAL(link.completed, 0);
            BOOST_CHECK(link.failedPayloads.front() == bytes);
        }
        else {
            BOOST_REQUIRE(link.Until([&]() { return link.completed == 1; }));
            BOOST_CHECK_EQUAL(link.failed, 0);
        }
        BOOST_CHECK_EQUAL(released.load(), 1);
        BOOST_CHECK(link.Empty());
    }
}

BOOST_AUTO_TEST_CASE(TcpclV3NoAckBundleSourceLoopback) {
    using boost::asio::ip::tcp;
    boost::asio::io_service io;
    tcp::acceptor acceptor(io, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
    acceptor.non_blocking(true);
    tcp::socket peer(io);
    std::vector<uint8_t> wire;
    std::atomic<unsigned int> completed(0);
    std::atomic<bool> wrongUserData(false);
    TcpclBundleSource source(0, 1, "ipn:2.0", 2, 5);
    source.BaseClass_SetOnSuccessfulBundleSendCallback([&](std::vector<uint8_t>& data, uint64_t) {
        const unsigned int id = completed.load();
        if (data != std::vector<uint8_t>(1, id)) wrongUserData.store(true);
        completed.fetch_add(1, std::memory_order_release);
    });
    source.Connect("127.0.0.1", std::to_string(acceptor.local_endpoint().port()));
    const auto waitFor = [&](const std::function<bool()>& done) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        do {
            if (done()) return true;
            if (peer.is_open() && peer.non_blocking()) {
                uint8_t bytes[8192];
                boost::system::error_code error;
                const std::size_t n = peer.read_some(boost::asio::buffer(bytes), error);
                wire.insert(wire.end(), bytes, bytes + n);
                BOOST_REQUIRE(!error || error == boost::asio::error::would_block || error == boost::asio::error::try_again);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    };
    BOOST_REQUIRE(waitFor([&]() {
        boost::system::error_code error;
        acceptor.accept(peer, error);
        BOOST_REQUIRE(!error || error == boost::asio::error::would_block || error == boost::asio::error::try_again);
        return !error;
    }));
    peer.non_blocking(true);
    BOOST_REQUIRE(waitFor([&]() { return wire.size() == 16; }));
    const std::vector<uint8_t> contact = {'d', 't', 'n', '!', 3, 0, 0, 0, 7, 'i', 'p', 'n', ':', '2', '.', '0'};
    boost::asio::write(peer, boost::asio::buffer(contact));
    BOOST_REQUIRE(waitFor([&]() { return source.ReadyToForward(); }));
    for (unsigned int id = 0; id < 21; ++id) {
        BOOST_REQUIRE(waitFor([&]() { return source.BaseClass_GetTotalBundlesUnacked() < 2; }));
        const std::vector<uint8_t> bytes = Payload(id);
        padded_vector_uint8_t bundle(bytes.begin(), bytes.end());
        BOOST_REQUIRE(source.BaseClass_Forward(bundle, {static_cast<uint8_t>(id)}));
    }
    BOOST_REQUIRE(waitFor([&]() { return completed.load(std::memory_order_acquire) == 21 && wire.size() == 16 + 21 * 33; }));
    BOOST_CHECK(!wrongUserData.load());
    BOOST_CHECK_EQUAL(source.BaseClass_GetTotalBundlesUnacked(), 0);
    BOOST_CHECK_EQUAL(source.BaseClass_GetTotalBundleBytesUnacked(), 0);
    BOOST_CHECK_EQUAL(source.BaseClass_GetTotalBundlesAcked(), 0);
    CheckWire(wire, 21, 5, false);
    source.Stop();
}
