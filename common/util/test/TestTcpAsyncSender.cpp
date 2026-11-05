// SPDX-License-Identifier: Apache-2.0

#include <boost/test/unit_test.hpp>
#include "TcpAsyncSender.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#ifdef OPENSSL_SUPPORT_ENABLED
#include <openssl/x509v3.h>
#endif

namespace {
enum class Transport { Plain, SslPlain, Tls };

// Exercise the shared sender with real loopback sockets. TLS uses an ephemeral
// certificate trusted by this client, including IP verification; no external
// certificate files, wall-clock expiry dates or verification bypass are needed.
class SenderPair {
public:
    explicit SenderPair(Transport transport) : released(0), controlCompleted(0),
        writeErrors(0), mode(transport),
#ifdef OPENSSL_SUPPORT_ENABLED
        clientContext(boost::asio::ssl::context::tls_client),
        serverContext(boost::asio::ssl::context::tls_server),
#endif
        peer(io) {
        using boost::asio::ip::tcp;
        tcp::acceptor acceptor(io, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
        if (mode == Transport::Plain) {
            socket = std::make_shared<tcp::socket>(io);
            socket->connect(acceptor.local_endpoint());
            acceptor.accept(peer);
            plain.reset(new TcpAsyncSender(socket, io));
            Register(*plain);
        }
#ifdef OPENSSL_SUPPORT_ENABLED
        else {
            if (mode == Transport::Tls) ConfigureCertificate();
            stream = std::make_shared<boost::asio::ssl::stream<tcp::socket> >(io, clientContext);
            tlsPeer.reset(new boost::asio::ssl::stream<tcp::socket>(io, serverContext));
            stream->next_layer().connect(acceptor.local_endpoint());
            acceptor.accept(tlsPeer->next_layer());
            if (mode == Transport::Tls) {
                stream->set_verify_mode(boost::asio::ssl::verify_peer);
                BOOST_REQUIRE_EQUAL(X509_VERIFY_PARAM_set1_ip_asc(
                    SSL_get0_param(stream->native_handle()), "127.0.0.1"), 1);
                unsigned int handshakes = 0;
                stream->async_handshake(boost::asio::ssl::stream_base::client,
                    [&](const boost::system::error_code& error) {
                        BOOST_REQUIRE_MESSAGE(!error, error.message()); ++handshakes;
                    });
                tlsPeer->async_handshake(boost::asio::ssl::stream_base::server,
                    [&](const boost::system::error_code& error) {
                        BOOST_REQUIRE_MESSAGE(!error, error.message()); ++handshakes;
                    });
                BOOST_REQUIRE(Wait([&]() { return handshakes == 2; }, false));
                BOOST_CHECK_EQUAL(SSL_get_verify_result(stream->native_handle()), X509_V_OK);
            }
            ssl.reset(new TcpAsyncSenderSsl(stream, io));
            Register(*ssl);
        }
#endif
        PeerSocket().non_blocking(true);
        successCallback = [this](const boost::system::error_code& error, std::size_t bytes,
            TcpAsyncSenderElement* element) {
            if (error) { ++writeErrors; return; }
            if (element->m_userData.empty()) { ++controlCompleted; return; }
            BOOST_CHECK_EQUAL(bytes, element->m_constBufferVec.front().size());
            successes.push_back(element->m_userData.front());
        };
        failureHook = [this](TcpAsyncSenderElement* element) {
            BOOST_REQUIRE(!element->m_userData.empty());
            hooks.push_back(element->m_userData.front());
        };
    }

    ~SenderPair() {
        boost::system::error_code ignored;
        PeerSocket().close(ignored);
        Socket().close(ignored);
        io.reset();
        while (io.poll()) {}
    }

    void Queue(uint8_t id, bool zmqData, std::size_t size, bool hook = true) {
        std::unique_ptr<TcpAsyncSenderElement> element(new TcpAsyncSenderElement);
        element->m_userData = {id};
        element->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &successCallback;
        if (hook) element->m_onFailedSendCallbackByIoServiceThreadPtr = &failureHook;
        if (zmqData) {
            uint8_t* bytes = new uint8_t[size];
            std::fill(bytes, bytes + size, id);
            element->m_underlyingDataZmqBundle.reset(new zmq::message_t(bytes, size,
                [](void* data, void* hint) {
                    delete[] static_cast<uint8_t*>(data);
                    static_cast<std::atomic<unsigned int>*>(hint)->fetch_add(1);
                }, &released));
            element->m_constBufferVec.push_back(boost::asio::buffer(bytes, size));
        }
        else {
            element->m_underlyingDataVecBundle.assign(size, id);
            element->m_constBufferVec.push_back(boost::asio::buffer(element->m_underlyingDataVecBundle));
        }
        Send(element.release());
    }

    void QueueControl() {
        TcpAsyncSenderElement* element = new TcpAsyncSenderElement;
        element->m_underlyingDataVecHeaders = {{0x40}};
        element->m_constBufferVec.push_back(boost::asio::buffer(element->m_underlyingDataVecHeaders[0]));
        element->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &successCallback;
        Send(element);
    }

    void SmallBuffers() {
        Socket().set_option(boost::asio::socket_base::send_buffer_size(1024));
        PeerSocket().set_option(boost::asio::socket_base::receive_buffer_size(1024));
    }

    void Interrupt(bool localClose) {
        if (localClose) Socket().close();
        else {
            PeerSocket().set_option(boost::asio::socket_base::linger(true, 0));
            PeerSocket().close();
        }
    }

    bool Wait(const std::function<bool()>& done, bool read = true) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        do {
            io.reset(); io.poll();
            if (read) Read();
            if (done()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }

    std::size_t Available() { return PeerSocket().available(); }
    std::vector<uint8_t> successes, failures, hooks, wire;
    std::atomic<unsigned int> released;
    unsigned int controlCompleted, writeErrors;

private:
    template <typename Sender> void Register(Sender& sender) {
        sender.SetUserAssignedUuid(73);
        sender.SetOnFailedBundleVecSendCallback([this](padded_vector_uint8_t& bytes,
            std::vector<uint8_t>& data, uint64_t uuid, bool successCalled) {
            Failed(bytes.data(), bytes.size(), data, uuid, successCalled);
        });
        sender.SetOnFailedBundleZmqSendCallback([this](zmq::message_t& bytes,
            std::vector<uint8_t>& data, uint64_t uuid, bool successCalled) {
            Failed(static_cast<const uint8_t*>(bytes.data()), bytes.size(), data, uuid, successCalled);
        });
    }

    void Failed(const uint8_t* bytes, std::size_t size, const std::vector<uint8_t>& data,
        uint64_t uuid, bool successCalled) {
        BOOST_CHECK_EQUAL(uuid, uint64_t(73));
        BOOST_CHECK(!successCalled);
        BOOST_REQUIRE_EQUAL(data.size(), std::size_t(1));
        BOOST_REQUIRE(size > 0);
        BOOST_CHECK(std::all_of(bytes, bytes + size, [&](uint8_t byte) { return byte == data.front(); }));
        failures.push_back(data.front());
        // When installed, retirement happens before the original payload-return
        // callback; callbacks of existing consumers need no new hook.
        if (!hooks.empty()) BOOST_CHECK(hooks == failures);
    }

    boost::asio::ip::tcp::socket& Socket() {
#ifdef OPENSSL_SUPPORT_ENABLED
        if (mode != Transport::Plain) return stream->next_layer();
#endif
        return *socket;
    }
    boost::asio::ip::tcp::socket& PeerSocket() {
#ifdef OPENSSL_SUPPORT_ENABLED
        if (mode != Transport::Plain) return tlsPeer->next_layer();
#endif
        return peer;
    }
    void Send(TcpAsyncSenderElement* element) {
#ifdef OPENSSL_SUPPORT_ENABLED
        if (mode == Transport::Tls) { ssl->AsyncSendSecure_ThreadSafe(element); return; }
        if (mode == Transport::SslPlain) { ssl->AsyncSendUnsecure_ThreadSafe(element); return; }
#endif
        plain->AsyncSend_ThreadSafe(element);
    }
    void Read() {
        uint8_t bytes[8192];
        boost::system::error_code error;
        std::size_t count;
#ifdef OPENSSL_SUPPORT_ENABLED
        if (mode == Transport::Tls) count = tlsPeer->read_some(boost::asio::buffer(bytes), error);
        else
#endif
            count = PeerSocket().read_some(boost::asio::buffer(bytes), error);
        BOOST_REQUIRE_MESSAGE(!error || error == boost::asio::error::would_block ||
            error == boost::asio::error::try_again, error.message());
        wire.insert(wire.end(), bytes, bytes + count);
    }

#ifdef OPENSSL_SUPPORT_ENABLED
    void ConfigureCertificate() {
        std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> context(
            EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL), EVP_PKEY_CTX_free);
        BOOST_REQUIRE(context);
        BOOST_REQUIRE_EQUAL(EVP_PKEY_keygen_init(context.get()), 1);
        BOOST_REQUIRE_EQUAL(EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 2048), 1);
        EVP_PKEY* rawKey = NULL;
        BOOST_REQUIRE_EQUAL(EVP_PKEY_keygen(context.get(), &rawKey), 1);
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(rawKey, EVP_PKEY_free);
        std::unique_ptr<X509, decltype(&X509_free)> cert(X509_new(), X509_free);
        BOOST_REQUIRE(cert);
        BOOST_REQUIRE_EQUAL(X509_set_version(cert.get(), 2), 1);
        BOOST_REQUIRE_EQUAL(ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1), 1);
        BOOST_REQUIRE(X509_gmtime_adj(X509_get_notBefore(cert.get()), -60));
        BOOST_REQUIRE(X509_gmtime_adj(X509_get_notAfter(cert.get()), 3600));
        BOOST_REQUIRE_EQUAL(X509_set_pubkey(cert.get(), key.get()), 1);
        X509_NAME* name = X509_get_subject_name(cert.get());
        BOOST_REQUIRE_EQUAL(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
            reinterpret_cast<const unsigned char*>("127.0.0.1"), -1, -1, 0), 1);
        BOOST_REQUIRE_EQUAL(X509_set_issuer_name(cert.get(), name), 1);
        std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> san(
            X509V3_EXT_conf_nid(NULL, NULL, NID_subject_alt_name,
                const_cast<char*>("IP:127.0.0.1")), X509_EXTENSION_free);
        BOOST_REQUIRE(san);
        BOOST_REQUIRE_EQUAL(X509_add_ext(cert.get(), san.get(), -1), 1);
        BOOST_REQUIRE(X509_sign(cert.get(), key.get(), EVP_sha256()) > 0);
        BOOST_REQUIRE_EQUAL(SSL_CTX_use_certificate(serverContext.native_handle(), cert.get()), 1);
        BOOST_REQUIRE_EQUAL(SSL_CTX_use_PrivateKey(serverContext.native_handle(), key.get()), 1);
        BOOST_REQUIRE_EQUAL(X509_STORE_add_cert(
            SSL_CTX_get_cert_store(clientContext.native_handle()), cert.get()), 1);
    }
#endif
    Transport mode;
    boost::asio::io_service io;
#ifdef OPENSSL_SUPPORT_ENABLED
    boost::asio::ssl::context clientContext, serverContext;
    TcpAsyncSenderSsl::ssl_stream_sharedptr_t stream;
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > tlsPeer;
    std::unique_ptr<TcpAsyncSenderSsl> ssl;
#endif
    boost::asio::ip::tcp::socket peer;
    std::shared_ptr<boost::asio::ip::tcp::socket> socket;
    std::unique_ptr<TcpAsyncSender> plain;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t successCallback;
    TcpAsyncSenderElement::OnFailedSendCallbackByIoServiceThread_t failureHook;
};

void CheckSuccess(Transport mode) {
    SenderPair pair(mode);
    pair.QueueControl();
    std::vector<uint8_t> expected = {0x40};
    for (uint8_t id = 1; id <= 4; ++id) {
        pair.Queue(id, id % 2, 31);
        expected.insert(expected.end(), 31, id);
    }
    BOOST_REQUIRE(pair.Wait([&]() { return pair.successes.size() == 4 && pair.wire.size() == expected.size(); }));
    BOOST_CHECK(pair.successes == std::vector<uint8_t>({1, 2, 3, 4}));
    BOOST_CHECK(pair.wire == expected);
    BOOST_CHECK(pair.failures.empty());
    BOOST_CHECK(pair.hooks.empty());
    BOOST_CHECK_EQUAL(pair.controlCompleted, 1U);
    BOOST_CHECK_EQUAL(pair.writeErrors, 0U);
    BOOST_CHECK_EQUAL(pair.released.load(), 2U);
}

void CheckInterrupted(Transport mode, bool zmqData, bool localClose, bool installHook = true) {
    SenderPair pair(mode);
    pair.SmallBuffers();
    for (uint8_t id = 1; id <= 3; ++id) pair.Queue(id, zmqData, 4 * 1024 * 1024, installHook);
    BOOST_REQUIRE(pair.Wait([&]() { return pair.Available() > 100; }, false));
    BOOST_CHECK(pair.successes.empty());
    pair.Interrupt(localClose);
    BOOST_REQUIRE(pair.Wait([&]() { return pair.failures.size() == 3; }, false));
    BOOST_CHECK(pair.failures == std::vector<uint8_t>({1, 2, 3}));
    BOOST_CHECK(pair.successes.empty());
    BOOST_CHECK_EQUAL(pair.writeErrors, 1U);
    // A send posted after the transport failed is returned without starting
    // another write, with exactly the same terminal and ownership semantics.
    pair.Queue(4, zmqData, 31, installHook);
    BOOST_REQUIRE(pair.Wait([&]() { return pair.failures.size() == 4; }, false));
    BOOST_CHECK(pair.failures == std::vector<uint8_t>({1, 2, 3, 4}));
    BOOST_CHECK_EQUAL(pair.writeErrors, 1U);
    if (installHook) BOOST_CHECK(pair.hooks == pair.failures);
    else BOOST_CHECK(pair.hooks.empty());
    BOOST_CHECK_EQUAL(pair.released.load(), zmqData ? 4U : 0U);
}

void CheckLifecycle(Transport mode) {
    CheckSuccess(mode);
    for (bool zmqData : {false, true}) {
        CheckInterrupted(mode, zmqData, false);
        CheckInterrupted(mode, zmqData, true);
    }
    CheckInterrupted(mode, false, true, false);
}
} // namespace

BOOST_AUTO_TEST_CASE(TcpAsyncSenderPlainLifecycle) { CheckLifecycle(Transport::Plain); }
#ifdef OPENSSL_SUPPORT_ENABLED
BOOST_AUTO_TEST_CASE(TcpAsyncSenderSslPlainLifecycle) { CheckLifecycle(Transport::SslPlain); }
BOOST_AUTO_TEST_CASE(TcpAsyncSenderTlsLifecycle) { CheckLifecycle(Transport::Tls); }
#endif
