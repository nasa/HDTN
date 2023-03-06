/**
 * @file BeastWebsocketServer.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2022 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 * This source file is based on the example "Advanced server, flex (plain + SSL)" from
 * https://raw.githubusercontent.com/boostorg/beast/b07edea9d70ed5a613879ab94896ed9b7255f5a8/example/advanced/server-flex/advanced_server_flex.cpp
 * Which holds the following copyright and license:
 * Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include "BeastWebsocketServer.h"
#include "Logger.h"

static const hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::gui;


//https://raw.githubusercontent.com/boostorg/beast/b07edea9d70ed5a613879ab94896ed9b7255f5a8/example/advanced/server-flex/advanced_server_flex.cpp
//https://raw.githubusercontent.com/boostorg/beast/b07edea9d70ed5a613879ab94896ed9b7255f5a8/example/common/server_certificate.hpp

//////#include "example/common/server_certificate.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <cstddef>
#include <memory>

/*  Load a signed certificate into the ssl context, and configure
    the context for use with a server.

    For this to work with the browser or operating system, it is
    necessary to import the "Beast Test CA" certificate into
    the local certificate store, browser, or operating system
    depending on your environment Please see the documentation
    accompanying the Beast certificate for more details.
*/
inline
void
load_server_certificate(boost::asio::ssl::context& ctx)
{
    /*
        The certificate was generated from CMD.EXE on Windows 10 using:

        winpty openssl dhparam -out dh.pem 2048
        winpty openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 10000 -out cert.pem -subj "//C=US\ST=CA\L=Los Angeles\O=Beast\CN=www.example.com"
    */

    std::string const cert =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDaDCCAlCgAwIBAgIJAO8vBu8i8exWMA0GCSqGSIb3DQEBCwUAMEkxCzAJBgNV\n"
        "BAYTAlVTMQswCQYDVQQIDAJDQTEtMCsGA1UEBwwkTG9zIEFuZ2VsZXNPPUJlYXN0\n"
        "Q049d3d3LmV4YW1wbGUuY29tMB4XDTE3MDUwMzE4MzkxMloXDTQ0MDkxODE4Mzkx\n"
        "MlowSTELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAkNBMS0wKwYDVQQHDCRMb3MgQW5n\n"
        "ZWxlc089QmVhc3RDTj13d3cuZXhhbXBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUA\n"
        "A4IBDwAwggEKAoIBAQDJ7BRKFO8fqmsEXw8v9YOVXyrQVsVbjSSGEs4Vzs4cJgcF\n"
        "xqGitbnLIrOgiJpRAPLy5MNcAXE1strVGfdEf7xMYSZ/4wOrxUyVw/Ltgsft8m7b\n"
        "Fu8TsCzO6XrxpnVtWk506YZ7ToTa5UjHfBi2+pWTxbpN12UhiZNUcrRsqTFW+6fO\n"
        "9d7xm5wlaZG8cMdg0cO1bhkz45JSl3wWKIES7t3EfKePZbNlQ5hPy7Pd5JTmdGBp\n"
        "yY8anC8u4LPbmgW0/U31PH0rRVfGcBbZsAoQw5Tc5dnb6N2GEIbq3ehSfdDHGnrv\n"
        "enu2tOK9Qx6GEzXh3sekZkxcgh+NlIxCNxu//Dk9AgMBAAGjUzBRMB0GA1UdDgQW\n"
        "BBTZh0N9Ne1OD7GBGJYz4PNESHuXezAfBgNVHSMEGDAWgBTZh0N9Ne1OD7GBGJYz\n"
        "4PNESHuXezAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQCmTJVT\n"
        "LH5Cru1vXtzb3N9dyolcVH82xFVwPewArchgq+CEkajOU9bnzCqvhM4CryBb4cUs\n"
        "gqXWp85hAh55uBOqXb2yyESEleMCJEiVTwm/m26FdONvEGptsiCmF5Gxi0YRtn8N\n"
        "V+KhrQaAyLrLdPYI7TrwAOisq2I1cD0mt+xgwuv/654Rl3IhOMx+fKWKJ9qLAiaE\n"
        "fQyshjlPP9mYVxWOxqctUdQ8UnsUKKGEUcVrA08i1OAnVKlPFjKBvk+r7jpsTPcr\n"
        "9pWXTO9JrYMML7d+XRSZA1n3856OqZDX4403+9FnXCvfcLZLLKTBvwwFgEFGpzjK\n"
        "UEVbkhd5qstF6qWK\n"
        "-----END CERTIFICATE-----\n";

    std::string const key =
        "-----BEGIN PRIVATE KEY-----\n"
        "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDJ7BRKFO8fqmsE\n"
        "Xw8v9YOVXyrQVsVbjSSGEs4Vzs4cJgcFxqGitbnLIrOgiJpRAPLy5MNcAXE1strV\n"
        "GfdEf7xMYSZ/4wOrxUyVw/Ltgsft8m7bFu8TsCzO6XrxpnVtWk506YZ7ToTa5UjH\n"
        "fBi2+pWTxbpN12UhiZNUcrRsqTFW+6fO9d7xm5wlaZG8cMdg0cO1bhkz45JSl3wW\n"
        "KIES7t3EfKePZbNlQ5hPy7Pd5JTmdGBpyY8anC8u4LPbmgW0/U31PH0rRVfGcBbZ\n"
        "sAoQw5Tc5dnb6N2GEIbq3ehSfdDHGnrvenu2tOK9Qx6GEzXh3sekZkxcgh+NlIxC\n"
        "Nxu//Dk9AgMBAAECggEBAK1gV8uETg4SdfE67f9v/5uyK0DYQH1ro4C7hNiUycTB\n"
        "oiYDd6YOA4m4MiQVJuuGtRR5+IR3eI1zFRMFSJs4UqYChNwqQGys7CVsKpplQOW+\n"
        "1BCqkH2HN/Ix5662Dv3mHJemLCKUON77IJKoq0/xuZ04mc9csykox6grFWB3pjXY\n"
        "OEn9U8pt5KNldWfpfAZ7xu9WfyvthGXlhfwKEetOuHfAQv7FF6s25UIEU6Hmnwp9\n"
        "VmYp2twfMGdztz/gfFjKOGxf92RG+FMSkyAPq/vhyB7oQWxa+vdBn6BSdsfn27Qs\n"
        "bTvXrGe4FYcbuw4WkAKTljZX7TUegkXiwFoSps0jegECgYEA7o5AcRTZVUmmSs8W\n"
        "PUHn89UEuDAMFVk7grG1bg8exLQSpugCykcqXt1WNrqB7x6nB+dbVANWNhSmhgCg\n"
        "VrV941vbx8ketqZ9YInSbGPWIU/tss3r8Yx2Ct3mQpvpGC6iGHzEc/NHJP8Efvh/\n"
        "CcUWmLjLGJYYeP5oNu5cncC3fXUCgYEA2LANATm0A6sFVGe3sSLO9un1brA4zlZE\n"
        "Hjd3KOZnMPt73B426qUOcw5B2wIS8GJsUES0P94pKg83oyzmoUV9vJpJLjHA4qmL\n"
        "CDAd6CjAmE5ea4dFdZwDDS8F9FntJMdPQJA9vq+JaeS+k7ds3+7oiNe+RUIHR1Sz\n"
        "VEAKh3Xw66kCgYB7KO/2Mchesu5qku2tZJhHF4QfP5cNcos511uO3bmJ3ln+16uR\n"
        "GRqz7Vu0V6f7dvzPJM/O2QYqV5D9f9dHzN2YgvU9+QSlUeFK9PyxPv3vJt/WP1//\n"
        "zf+nbpaRbwLxnCnNsKSQJFpnrE166/pSZfFbmZQpNlyeIuJU8czZGQTifQKBgHXe\n"
        "/pQGEZhVNab+bHwdFTxXdDzr+1qyrodJYLaM7uFES9InVXQ6qSuJO+WosSi2QXlA\n"
        "hlSfwwCwGnHXAPYFWSp5Owm34tbpp0mi8wHQ+UNgjhgsE2qwnTBUvgZ3zHpPORtD\n"
        "23KZBkTmO40bIEyIJ1IZGdWO32q79nkEBTY+v/lRAoGBAI1rbouFYPBrTYQ9kcjt\n"
        "1yfu4JF5MvO9JrHQ9tOwkqDmNCWx9xWXbgydsn/eFtuUMULWsG3lNjfst/Esb8ch\n"
        "k5cZd6pdJZa4/vhEwrYYSuEjMCnRb0lUsm7TsHxQrUd6Fi/mUuFU/haC0o0chLq7\n"
        "pVOUFq5mW8p0zbtfHbjkgxyF\n"
        "-----END PRIVATE KEY-----\n";

    std::string const dh =
        "-----BEGIN DH PARAMETERS-----\n"
        "MIIBCAKCAQEArzQc5mpm0Fs8yahDeySj31JZlwEphUdZ9StM2D8+Fo7TMduGtSi+\n"
        "/HRWVwHcTFAgrxVdm+dl474mOUqqaz4MpzIb6+6OVfWHbQJmXPepZKyu4LgUPvY/\n"
        "4q3/iDMjIS0fLOu/bLuObwU5ccZmDgfhmz1GanRlTQOiYRty3FiOATWZBRh6uv4u\n"
        "tff4A9Bm3V9tLx9S6djq31w31Gl7OQhryodW28kc16t9TvO1BzcV3HjRPwpe701X\n"
        "oEEZdnZWANkkpR/m/pfgdmGPU66S2sXMHgsliViQWpDCYeehrvFRHEdR9NV+XJfC\n"
        "QMUk26jPTIVTLfXmmwU0u8vUkpR7LQKkwwIBAg==\n"
        "-----END DH PARAMETERS-----\n";

    ctx.set_password_callback(
        [](std::size_t,
            boost::asio::ssl::context_base::password_purpose)
        {
            return "test";
        });

    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::single_dh_use);

    ctx.use_certificate_chain(
        boost::asio::buffer(cert.data(), cert.size()));

    ctx.use_private_key(
        boost::asio::buffer(key.data(), key.size()),
        boost::asio::ssl::context::file_format::pem);

    ctx.use_tmp_dh(
        boost::asio::buffer(dh.data(), dh.size()));
}


#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/make_unique.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <boost/thread.hpp>
#include "ThreadNamer.h"
#include <vector>




struct ServerState : private boost::noncopyable {
    ServerState() = delete;
    ServerState(const std::string& docRoot,
        const OnNewBeastWebsocketConnectionCallback_t& connectionCallback,
        const OnNewBeastWebsocketDataReceivedCallback_t& dataCallback) :
        m_docRoot(docRoot),
        m_onNewWebsocketConnectionCallback(connectionCallback),
        m_onNewWebsocketDataReceivedCallback(dataCallback),
        m_nextWebsocketConnectionId(0) {}

    const std::string m_docRoot;
    const OnNewBeastWebsocketConnectionCallback_t m_onNewWebsocketConnectionCallback;
    const OnNewBeastWebsocketDataReceivedCallback_t m_onNewWebsocketDataReceivedCallback;
    boost::mutex m_activeConnectionsMutex;
    uint32_t m_nextWebsocketConnectionId;
    typedef std::map<uint32_t, std::shared_ptr<WebsocketSessionBase> > active_connections_map_t;
    active_connections_map_t m_activeConnections; //allow multiple connections
};
typedef std::shared_ptr<ServerState> ServerState_ptr;

static boost::beast::string_view GetExtension(const boost::beast::string_view& path) {
    const std::size_t pos = path.rfind(".");
    if (pos == boost::beast::string_view::npos) {
        return boost::beast::string_view{};
    }
    return path.substr(pos);
}

// Return a reasonable mime type based on the extension of a file.
static boost::beast::string_view MimeType(const boost::beast::string_view& path) {
    const boost::beast::string_view ext = GetExtension(path);
    if (boost::beast::iequals(ext, ".htm"))  return "text/html";
    if (boost::beast::iequals(ext, ".html")) return "text/html";
    if (boost::beast::iequals(ext, ".php"))  return "text/html";
    if (boost::beast::iequals(ext, ".css"))  return "text/css";
    if (boost::beast::iequals(ext, ".txt"))  return "text/plain";
    if (boost::beast::iequals(ext, ".js"))   return "application/javascript";
    if (boost::beast::iequals(ext, ".json")) return "application/json";
    if (boost::beast::iequals(ext, ".xml"))  return "application/xml";
    if (boost::beast::iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if (boost::beast::iequals(ext, ".flv"))  return "video/x-flv";
    if (boost::beast::iequals(ext, ".png"))  return "image/png";
    if (boost::beast::iequals(ext, ".jpe"))  return "image/jpeg";
    if (boost::beast::iequals(ext, ".jpeg")) return "image/jpeg";
    if (boost::beast::iequals(ext, ".jpg"))  return "image/jpeg";
    if (boost::beast::iequals(ext, ".gif"))  return "image/gif";
    if (boost::beast::iequals(ext, ".bmp"))  return "image/bmp";
    if (boost::beast::iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if (boost::beast::iequals(ext, ".tiff")) return "image/tiff";
    if (boost::beast::iequals(ext, ".tif"))  return "image/tiff";
    if (boost::beast::iequals(ext, ".svg"))  return "image/svg+xml";
    if (boost::beast::iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
static std::string path_cat(const boost::beast::string_view& base, const boost::beast::string_view& path) {
    if (base.empty()) {
        return std::string(path);
    }
    std::string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if (result.back() == path_separator) {
        result.resize(result.size() - 1);
    }
    result.append(path.data(), path.size());
    for (char& c : result) {
        if (c == '/') {
            c = path_separator;
        }
    }
#else
    char constexpr path_separator = '/';
    if (result.back() == path_separator) {
        result.resize(result.size() - 1);
    }
    result.append(path.data(), path.size());
#endif
    return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<class Body, class Allocator, class Send>
void handle_request(boost::beast::string_view doc_root,
    boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator> >&& req,
    Send&& send)
{
    // Returns a bad request response
    auto const bad_request = [&req](boost::beast::string_view why) {
        boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::bad_request, req.version() };
        res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found = [&req](boost::beast::string_view target) {
        boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::not_found, req.version() };
        res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error = [&req](boost::beast::string_view what) {
        boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::internal_server_error, req.version() };
        res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if ((req.method() != boost::beast::http::verb::get) && (req.method() != boost::beast::http::verb::head)) {
        return send(bad_request("Unknown HTTP-method"));
    }

    // Request path must be absolute and not contain "..".
    if (req.target().empty() || (req.target()[0] != '/') || (req.target().find("..") != boost::beast::string_view::npos)) {
        return send(bad_request("Illegal request-target"));
    }

    // Build the path to the requested file
    std::string path = path_cat(doc_root, req.target());
    if (req.target().back() == '/') {
        path.append("index.html");
    }

    // Attempt to open the file
    boost::beast::error_code ec;
    boost::beast::http::file_body::value_type body;
    body.open(path.c_str(), boost::beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if (ec == boost::beast::errc::no_such_file_or_directory) {
        return send(not_found(req.target()));
    }

    // Handle an unknown error
    if (ec) {
        return send(server_error(ec.message()));
    }

    // Cache the size since we need it after the move
    const uint64_t size = body.size();

    // Respond to HEAD request
    if (req.method() == boost::beast::http::verb::head) {
        boost::beast::http::response<boost::beast::http::empty_body> res{ boost::beast::http::status::ok, req.version() };
        res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::content_type, MimeType(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    // Respond to GET request
    boost::beast::http::response<boost::beast::http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(boost::beast::http::status::ok, req.version()) };
    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(boost::beast::http::field::content_type, MimeType(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
}

//------------------------------------------------------------------------------

// Report a failure
void fail(boost::beast::error_code ec, char const* what) {
    // boost::asio::ssl::error::stream_truncated, also known as an SSL "short read",
    // indicates the peer closed the connection without performing the
    // required closing handshake (for example, Google does this to
    // improve performance). Generally this can be a security issue,
    // but if your communication protocol is self-terminated (as
    // it is with both HTTP and WebSocket) then you may simply
    // ignore the lack of close_notify.
    //
    // https://github.com/boostorg/beast/issues/38
    //
    // https://security.stackexchange.com/questions/91435/how-to-handle-a-malicious-ssl-tls-shutdown
    //
    // When a short read would cut off the end of an HTTP message,
    // Beast returns the error boost::beast::http::error::partial_message.
    // Therefore, if we see a short read here, it has occurred
    // after the message has been completed, so it is safe to ignore it.

    if (ec == boost::asio::ssl::error::stream_truncated) {
        return;
    }

    LOG_ERROR(subprocess) << what << ": " << ec.message() << "\n";
}

//------------------------------------------------------------------------------



// Echoes back all received WebSocket messages.
// This uses the Curiously Recurring Template Pattern so that
// the same code works with both SSL streams and regular sockets.
template<class Derived>
class websocket_session : public WebsocketSessionBase {
public:
    virtual ~websocket_session() override { std::cout << "~~websocket_session\n"; } //for base shared_ptr
protected:
    explicit websocket_session(uint32_t uniqueId, ServerState_ptr& serverStatePtr) :
        WebsocketSessionBase(uniqueId),
        m_serverStatePtr(serverStatePtr),
        m_writeInProgress(false),
        m_sendErrorOccurred(false) {}
private:
    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived& derived() {
        return static_cast<Derived&>(*this);
    }

    
    
    boost::beast::flat_buffer m_flatBuffer;
    ServerState_ptr m_serverStatePtr;
    std::queue<std::shared_ptr<std::string> > m_queueDataToSend;
    volatile bool m_writeInProgress;
    volatile bool m_sendErrorOccurred;

    // Start the asynchronous operation
    template<class Body, class Allocator>
    void do_accept(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>> req) {
        // Set suggested timeout settings for the websocket
        derived().ws().set_option(
            boost::beast::websocket::stream_base::timeout::suggested(
                boost::beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        derived().ws().set_option(
            boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::response_type& res) {
                    res.set(boost::beast::http::field::server, std::string(BOOST_BEAST_VERSION_STRING));
                }));

        // Accept the websocket handshake
        derived().ws().async_accept(
            req,
            boost::beast::bind_front_handler(&websocket_session::on_accept, derived().shared_from_this()));
    }

    void on_accept(boost::beast::error_code ec) {
        std::cout << "ON ACCEPT\n";
        if (ec) {
            return fail(ec, "accept");
        }

        // Read a message
        do_read();

        if (m_serverStatePtr->m_onNewWebsocketConnectionCallback) {
            m_serverStatePtr->m_onNewWebsocketConnectionCallback(*this);
        }
    }

    void do_read() {
        std::cout << "DO READ\n";
        // Read a message into our buffer
        derived().ws().async_read(
            m_flatBuffer,
            boost::beast::bind_front_handler(
                &websocket_session::on_read,
                derived().shared_from_this()));
    }

    void on_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
        std::cout << "ON READ\n";
        boost::ignore_unused(bytes_transferred);

        // This indicates that the websocket_session was closed
        if (ec == boost::beast::websocket::error::closed) {
            std::cout << "calling lock1\n";
            boost::mutex::scoped_lock lock(m_serverStatePtr->m_activeConnectionsMutex);
            std::cout << "called lock1\n";
            if (m_serverStatePtr->m_activeConnections.erase(m_uniqueId) == 0) {
                LOG_ERROR(subprocess) << "cannot erase websocket id " << m_uniqueId << " from map";
            }
            else {
                std::cout << "erased from map\n";
            }
            return;
        }

        if (ec) {
            std::cout << "FAIL READ\n";
            fail(ec, "read");
            return;
        }

        if (derived().ws().got_text()) {
            if (m_serverStatePtr->m_onNewWebsocketDataReceivedCallback) {
                std::string s = boost::beast::buffers_to_string(m_flatBuffer.data());
                m_serverStatePtr->m_onNewWebsocketDataReceivedCallback(*this, s);
            }
        }

        // Echo the message
        //derived().ws().text(derived().ws().got_text());
        //derived().ws().async_write(
        //    m_flatBuffer.data(),
        //    boost::beast::bind_front_handler(
        //        &websocket_session::on_write,
        //        derived().shared_from_this()));

        // Clear the buffer
        m_flatBuffer.consume(m_flatBuffer.size());

        // Do another read
        do_read();
    }

    void on_write(boost::beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            fail(ec, "write");
            return;
        }
        /*
        // Clear the buffer
        m_flatBuffer.consume(m_flatBuffer.size());

        // Do another read
        do_read();
        */
    }

    void DoSendQueuedElement() {
        derived().ws().text(true); //set text mode
        derived().ws().async_write(
            boost::asio::buffer(*m_queueDataToSend.front()),
            boost::bind(
                &websocket_session::OnSentStringFromQueuedSharedPtr,
                derived().shared_from_this(), //copied by value
                boost::placeholders::_1, boost::placeholders::_2));
    }

    void OnSentStringFromQueuedSharedPtr(boost::beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        m_queueDataToSend.pop();
        if (ec) {
            m_sendErrorOccurred = true;
            fail(ec, "write");
        }
        else if (m_queueDataToSend.empty()) {
            m_writeInProgress = false;
        }
        else {
            DoSendQueuedElement();
        }
    }

    void OnClose(boost::beast::error_code ec) {
        std::cout << "onclose\n";
        if (ec) {
            fail(ec, "close");
        }
    }

    void HandleQueueAndSendTextData_NotThreadSafe(std::shared_ptr<std::string>& stringPtr) {
        if (!m_sendErrorOccurred) {
            m_queueDataToSend.push(std::move(stringPtr));
            if (!m_writeInProgress) {
                m_writeInProgress = true;
                DoSendQueuedElement();
            }
        }
    }

public:
    // Start the asynchronous operation
    template<class Body, class Allocator>
    void run(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator> > req) {
        // Accept the WebSocket upgrade request
        do_accept(std::move(req));
    }
    virtual void AsyncSendTextData(std::shared_ptr<std::string>&& stringPtr) override {
        boost::asio::post(derived().ws().get_executor(),
            boost::bind(&websocket_session::HandleQueueAndSendTextData_NotThreadSafe,
                derived().shared_from_this(), std::move(stringPtr)));
    }

    virtual void AsyncClose() override {
        derived().ws().async_close(boost::beast::websocket::close_code::none,
            boost::beast::bind_front_handler(
                &websocket_session::OnClose,
                derived().shared_from_this())); //copied by value
    }
};

//------------------------------------------------------------------------------

// Handles a plain WebSocket connection
class plain_websocket_session : public websocket_session<plain_websocket_session>,
    public std::enable_shared_from_this<plain_websocket_session>
{
    boost::beast::websocket::stream<boost::beast::tcp_stream> m_websocketStream;

public:
    virtual ~plain_websocket_session() override { std::cout << "~plain_websocket_session\n"; }
    // Create the session
    explicit plain_websocket_session(uint32_t uniqueId, boost::beast::tcp_stream&& tcpStream, ServerState_ptr& serverStatePtr) :
        websocket_session(uniqueId, serverStatePtr),
        m_websocketStream(std::move(tcpStream)) {}

    // Called by the base class
    boost::beast::websocket::stream<boost::beast::tcp_stream>& ws() {
        return m_websocketStream;
    }
};

//------------------------------------------------------------------------------

// Handles an SSL WebSocket connection
class ssl_websocket_session : public websocket_session<ssl_websocket_session>,
    public std::enable_shared_from_this<ssl_websocket_session>
{
    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream> > m_sslWebsocketStream;

public:
    virtual ~ssl_websocket_session() override {}
    // Create the ssl_websocket_session
    explicit ssl_websocket_session(uint32_t uniqueId, boost::beast::ssl_stream<boost::beast::tcp_stream>&& sslStream, ServerState_ptr& serverStatePtr) :
        websocket_session(uniqueId, serverStatePtr),
        m_sslWebsocketStream(std::move(sslStream)) {}

    // Called by the base class
    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream> >& ws() {
        return m_sslWebsocketStream;
    }
};

//------------------------------------------------------------------------------


template<class Body, class Allocator>
void make_websocket_session(boost::beast::tcp_stream stream, ServerState_ptr& serverStatePtr,
    boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator> > req)
{
    boost::mutex::scoped_lock lock(serverStatePtr->m_activeConnectionsMutex);
    const uint32_t uniqueId = serverStatePtr->m_nextWebsocketConnectionId++;
    std::shared_ptr<plain_websocket_session> session = std::make_shared<plain_websocket_session>(
        uniqueId, std::move(stream), serverStatePtr);
    session->run(std::move(req)); //creates shared_ptr copy
    std::pair<ServerState::active_connections_map_t::iterator, bool> ret =
        serverStatePtr->m_activeConnections.emplace(uniqueId, std::move(session));
}

template<class Body, class Allocator>
void make_websocket_session(boost::beast::ssl_stream<boost::beast::tcp_stream> stream, ServerState_ptr& serverStatePtr,
    boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator> > req)
{

    boost::mutex::scoped_lock lock(serverStatePtr->m_activeConnectionsMutex);
    const uint32_t uniqueId = serverStatePtr->m_nextWebsocketConnectionId++;
    std::shared_ptr<ssl_websocket_session> session = std::make_shared<ssl_websocket_session>(
        uniqueId, std::move(stream), serverStatePtr);
    session->run(std::move(req)); //creates shared_ptr copy
    std::pair<ServerState::active_connections_map_t::iterator, bool> ret =
        serverStatePtr->m_activeConnections.emplace(uniqueId, std::move(session));
}

//------------------------------------------------------------------------------

// Handles an HTTP server connection.
// This uses the Curiously Recurring Template Pattern so that
// the same code works with both SSL streams and regular sockets.
template<class Derived>
class http_session {
    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived& derived() {
        return static_cast<Derived&>(*this);
    }

    // This queue is used for HTTP pipelining.
    class queue {
        enum {
            // Maximum number of responses we will queue
            limit = 8
        };

        // The type-erased, saved work item
        struct work {
            virtual ~work() = default;
            virtual void operator()() = 0;
        };

        http_session& self_;
        std::vector<std::unique_ptr<work> > items_;

    public:
        explicit queue(http_session& self) : self_(self) {
            static_assert(limit > 0, "queue limit must be positive");
            items_.reserve(limit);
        }

        // Returns `true` if we have reached the queue limit
        bool is_full() const noexcept {
            return items_.size() >= limit;
        }

        // Called when a message finishes sending
        // Returns `true` if the caller should initiate a read
        bool on_write() {
            BOOST_ASSERT(!items_.empty());
            const bool was_full = is_full();
            items_.erase(items_.begin());
            if (!items_.empty()) {
                (*items_.front())();
            }
            return was_full;
        }

        // Called by the HTTP handler to send a response.
        template<bool isRequest, class Body, class Fields>
        void operator()(boost::beast::http::message<isRequest, Body, Fields>&& msg) {
            // This holds a work item
            struct work_impl : work
            {
                http_session& self_;
                boost::beast::http::message<isRequest, Body, Fields> m_httpMessage;

                work_impl(http_session& self, boost::beast::http::message<isRequest, Body, Fields>&& msg) :
                    self_(self),
                    m_httpMessage(std::move(msg)) {}

                void operator()() {
                    boost::beast::http::async_write(
                        self_.derived().stream(),
                        m_httpMessage,
                        boost::beast::bind_front_handler(
                            &http_session::on_write,
                            self_.derived().shared_from_this(),
                            m_httpMessage.need_eof()));
                }
            };

            // Allocate and store the work
            items_.push_back(boost::make_unique<work_impl>(self_, std::move(msg)));

            // If there was no previous work, start this one
            if (items_.size() == 1) {
                (*items_.front())();
            }
        }
    };

    ServerState_ptr m_serverStatePtr;
    queue m_queue;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    boost::optional<boost::beast::http::request_parser<boost::beast::http::string_body> > parser_;

protected:
    boost::beast::flat_buffer m_flatBuffer;

public:
    // Construct the session
    http_session(boost::beast::flat_buffer buffer, ServerState_ptr& serverStatePtr) :
        m_serverStatePtr(serverStatePtr),
        m_queue(*this),
        m_flatBuffer(std::move(buffer)) {}

    void do_read() {
        // Construct a new parser for each message
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser_->body_limit(10000);

        // Set the timeout.
        boost::beast::get_lowest_layer(derived().stream()).expires_after(std::chrono::seconds(30));

        // Read a request using the parser-oriented interface
        boost::beast::http::async_read(
            derived().stream(),
            m_flatBuffer,
            *parser_,
            boost::beast::bind_front_handler(
                &http_session::on_read,
                derived().shared_from_this()));
    }

    void on_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == boost::beast::http::error::end_of_stream) {
            derived().do_eof();
            return;
        }

        if (ec) {
            fail(ec, "read");
            return;
        }

        // See if it is a WebSocket Upgrade
        if (boost::beast::websocket::is_upgrade(parser_->get())) {
            // Disable the timeout.
            // The boost::beast::websocket::stream uses its own timeout settings.
            boost::beast::get_lowest_layer(derived().stream()).expires_never();

            // Create a websocket session, transferring ownership
            // of both the socket and the HTTP request.
            make_websocket_session(derived().release_stream(), m_serverStatePtr, parser_->release());
            return;
        }

        // Send the response
        handle_request(m_serverStatePtr->m_docRoot, parser_->release(), m_queue);

        // If we aren't at the queue limit, try to pipeline another request
        if (!m_queue.is_full()) {
            do_read();
        }
    }

    void on_write(bool close, boost::beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            fail(ec, "write");
            return;
        }

        if (close) {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            derived().do_eof();
            return;
        }

        // Inform the queue that a write completed
        if (m_queue.on_write()) {
            // Read another request
            do_read();
        }
    }
};

//------------------------------------------------------------------------------

// Handles a plain HTTP connection
class plain_http_session : public http_session<plain_http_session>,
    public std::enable_shared_from_this<plain_http_session>
{
    boost::beast::tcp_stream m_tcpStream;

public:
    // Create the session
    plain_http_session(boost::beast::tcp_stream&& stream, boost::beast::flat_buffer&& buffer,
        ServerState_ptr& serverStatePtr) :
        http_session<plain_http_session>(std::move(buffer), serverStatePtr),
        m_tcpStream(std::move(stream)) {}

    // Start the session
    void run() {
        this->do_read();
    }

    // Called by the base class
    boost::beast::tcp_stream& stream() {
        return m_tcpStream;
    }

    // Called by the base class
    boost::beast::tcp_stream release_stream() {
        return std::move(m_tcpStream);
    }

    // Called by the base class
    void do_eof() {
        // Send a TCP shutdown
        boost::beast::error_code ec;
        m_tcpStream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Handles an SSL HTTP connection
class ssl_http_session : public http_session<ssl_http_session>,
    public std::enable_shared_from_this<ssl_http_session>
{
    boost::beast::ssl_stream<boost::beast::tcp_stream> m_sslStream;

public:
    // Create the http_session
    ssl_http_session(boost::beast::tcp_stream&& stream, boost::asio::ssl::context& ctx,
        boost::beast::flat_buffer&& buffer, ServerState_ptr& serverStatePtr) :
        http_session<ssl_http_session>(std::move(buffer), serverStatePtr),
        m_sslStream(std::move(stream), ctx) {}

    // Start the session
    void run() {
        // Set the timeout.
        boost::beast::get_lowest_layer(m_sslStream).expires_after(std::chrono::seconds(30));

        // Perform the SSL handshake
        // Note, this is the buffered version of the handshake.
        m_sslStream.async_handshake(
            boost::asio::ssl::stream_base::server,
            m_flatBuffer.data(),
            boost::beast::bind_front_handler(
                &ssl_http_session::on_handshake,
                shared_from_this()));
    }

    // Called by the base class
    boost::beast::ssl_stream<boost::beast::tcp_stream>& stream() {
        return m_sslStream;
    }

    // Called by the base class
    boost::beast::ssl_stream<boost::beast::tcp_stream> release_stream() {
        return std::move(m_sslStream);
    }

    // Called by the base class
    void do_eof() {
        // Set the timeout.
        boost::beast::get_lowest_layer(m_sslStream).expires_after(std::chrono::seconds(30));

        // Perform the SSL shutdown
        m_sslStream.async_shutdown(
            boost::beast::bind_front_handler(
                &ssl_http_session::on_shutdown,
                shared_from_this()));
    }

private:
    void on_handshake(boost::beast::error_code ec, std::size_t bytes_used) {
        if (ec) {
            fail(ec, "handshake");
            return;
        }

        // Consume the portion of the buffer used by the handshake
        m_flatBuffer.consume(bytes_used);

        do_read();
    }

    void on_shutdown(boost::beast::error_code ec) {
        if (ec) {
            fail(ec, "shutdown");
            return;
        }

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Detects SSL handshakes
class detect_session : public std::enable_shared_from_this<detect_session>
{
    boost::beast::tcp_stream stream_;
    boost::asio::ssl::context& m_sslContext;
    ServerState_ptr m_serverStatePtr;
    boost::beast::flat_buffer m_flatBuffer;

public:
    explicit detect_session(boost::asio::ip::tcp::socket&& socket,
        boost::asio::ssl::context& ctx, ServerState_ptr& serverState) :
        stream_(std::move(socket)),
        m_sslContext(ctx),
        m_serverStatePtr(serverState) {}

    // Launch the detector
    void run() {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        boost::asio::dispatch(
            stream_.get_executor(),
            boost::beast::bind_front_handler(
                &detect_session::on_run,
                this->shared_from_this()));
    }

    void on_run() {
        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        boost::beast::async_detect_ssl(
            stream_,
            m_flatBuffer,
            boost::beast::bind_front_handler(
                &detect_session::on_detect,
                this->shared_from_this()));
    }

    void on_detect(boost::beast::error_code ec, bool result) {
        if (ec) {
            fail(ec, "detect");
            return;
        }

        if (result) {
            // Launch SSL session
            std::shared_ptr<ssl_http_session> session = std::make_shared<ssl_http_session>(std::move(stream_), m_sslContext, std::move(m_flatBuffer), m_serverStatePtr);
            session->run();
        }
        else {
            // Launch plain session
            std::shared_ptr<plain_http_session> session = std::make_shared<plain_http_session>(std::move(stream_), std::move(m_flatBuffer), m_serverStatePtr);
            session->run();
        }
    }
};

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    boost::asio::io_service& m_ioService;
    boost::asio::ssl::context& m_sslContext;
    boost::asio::ip::tcp::acceptor acceptor_;
    ServerState_ptr m_serverStatePtr;

public:
    listener(boost::asio::io_service& ioc, boost::asio::ssl::context& ctx,
        boost::asio::ip::tcp::endpoint endpoint,
        ServerState_ptr& serverStatePtr) :
        m_ioService(ioc),
        m_sslContext(ctx),
        acceptor_(boost::asio::make_strand(ioc)),
        m_serverStatePtr(serverStatePtr)
    {
        boost::beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec) {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec) {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void run() {
        do_accept();
    }

private:
    void do_accept() {
        // The new connection gets its own strand
        acceptor_.async_accept(
            boost::asio::make_strand(m_ioService),
            boost::beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
        if (ec) {
            fail(ec, "accept");
        }
        else {
            // Create the detector http_session and run it
            std::shared_ptr<detect_session> detectSession = std::make_shared<detect_session>(std::move(socket), m_sslContext, m_serverStatePtr);
            detectSession->run();
        }

        // Accept another connection
        do_accept();
    }
};

struct BeastWebsocketServer::Impl : private boost::noncopyable {

    Impl() :
        m_ioService(), //(1) => concurrency_hint of 1 thread
        m_sslContext(boost::asio::ssl::context::tlsv12) {}
    ~Impl() {
        Stop();
    }
    void Stop() {
        if (m_serverStatePtr) {
            std::cout << "calling lockS\n";
            boost::mutex::scoped_lock lock(m_serverStatePtr->m_activeConnectionsMutex);
            std::cout << "called lockS\n";
            for (ServerState::active_connections_map_t::iterator it = m_serverStatePtr->m_activeConnections.begin();
                it != m_serverStatePtr->m_activeConnections.end(); ++it)
            {
                std::cout << "call async close\n";
                it->second->AsyncClose();
            }

            m_serverStatePtr->m_activeConnections.clear();
        }
        m_serverStatePtr.reset();
        boost::this_thread::sleep(boost::posix_time::seconds(2));
        m_ioService.stop();
        if (m_ioServiceThreadPtr) {
            try {
                m_ioServiceThreadPtr->join();
                m_ioServiceThreadPtr.reset(); //delete it
            }
            catch (const boost::thread_resource_error&) {
                LOG_ERROR(subprocess) << "error stopping BeastWebsocketServer io_service";
            }
        }
    }
    bool Init(const boost::filesystem::path& documentRoot, const std::string& portNumberAsString,
        const OnNewBeastWebsocketConnectionCallback_t& connectionCallback, const OnNewBeastWebsocketDataReceivedCallback_t& dataCallback)
    {
        // This holds the self-signed certificate used by the server
        load_server_certificate(m_sslContext);

        auto const address = boost::asio::ip::make_address("0.0.0.0");
        const uint16_t port = boost::lexical_cast<uint16_t>(portNumberAsString);
        m_serverStatePtr = std::make_shared<ServerState>(documentRoot.string(), connectionCallback, dataCallback);
        // Create and launch a listening port
        std::make_shared<listener>(m_ioService, m_sslContext, boost::asio::ip::tcp::endpoint(address, port), m_serverStatePtr)->run();

        //StartTcpAccept();
        m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
        ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceWebserver");

        LOG_INFO(subprocess) << "HDTN Webserver at http://localhost:" << portNumberAsString;

        return true;
    }
    void SendTextDataToActiveWebsockets(const std::shared_ptr<std::string>& stringPtr) {
        boost::mutex::scoped_lock lock(m_serverStatePtr->m_activeConnectionsMutex);
        for (ServerState::active_connections_map_t::iterator it = m_serverStatePtr->m_activeConnections.begin();
            it != m_serverStatePtr->m_activeConnections.end(); ++it)
        {
            it->second->AsyncSendTextData(std::shared_ptr<std::string>(stringPtr));
        }
    }

private:
    boost::asio::io_service m_ioService;
    // The SSL context is required, and holds certificates
    boost::asio::ssl::context m_sslContext;

    //must be shared since listener inherits from enable_shared_from_this
    // (actually not needed, ptr contained within its callbacks)
    //std::shared_ptr<listener> m_listenerSharedPtr; 

    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    ServerState_ptr m_serverStatePtr;
public:
    
};


///////////////////////////
//public facing functions
///////////////////////////
BeastWebsocketServer::BeastWebsocketServer() :
    m_pimpl(boost::make_unique<BeastWebsocketServer::Impl>()) {}
BeastWebsocketServer::~BeastWebsocketServer() {
    Stop();
}
bool BeastWebsocketServer::Init(const boost::filesystem::path& documentRoot, const std::string& portNumberAsString,
    const OnNewBeastWebsocketConnectionCallback_t& connectionCallback, const OnNewBeastWebsocketDataReceivedCallback_t& dataCallback)
{
    return m_pimpl->Init(documentRoot, portNumberAsString, connectionCallback, dataCallback);
}
void BeastWebsocketServer::Stop() {
    m_pimpl->Stop();
}
void BeastWebsocketServer::SendTextDataToActiveWebsockets(const std::shared_ptr<std::string>& stringPtr) {
    m_pimpl->SendTextDataToActiveWebsockets(stringPtr);
}

/*

void WebSocketHandler::SendTextDataToActiveWebsockets(const char* data, std::size_t size) {
    boost::mutex::scoped_lock lock(m_mutex);
    for (std::set<struct mg_connection*>::iterator it = m_activeConnections.begin(); it != m_activeConnections.end(); ++it) {
        mg_websocket_write(*it, MG_WEBSOCKET_OPCODE_TEXT, data, size);
    }
}

void WebSocketHandler::SendBinaryDataToActiveWebsockets(const char *data, std::size_t size) {
    boost::mutex::scoped_lock lock(m_mutex);
    for (std::set<struct mg_connection *>::iterator it = m_activeConnections.begin(); it != m_activeConnections.end(); ++it) {
        mg_websocket_write(*it, MG_WEBSOCKET_OPCODE_BINARY, data, size);
    }
}

bool WebSocketHandler::handleConnection(CivetServer *server, const struct mg_connection *conn) {
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.insert((struct mg_connection *)conn).second) {
        LOG_INFO(subprocess) << "WS connected";
        return true;
    }
    else {
        LOG_ERROR(subprocess) << "this WS is already connected";
        return false;
    }
}

void WebSocketHandler::handleReadyState(CivetServer *server, struct mg_connection *conn) {
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.count(conn) == 0) {
        LOG_ERROR(subprocess) << "error in handleReadyState, connections do not match";
        return;
    }
    LOG_INFO(subprocess) << "WS ready";

    const char *text = "Hello websocket";
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, text, strlen(text));

    if (m_onNewWebsocketConnectionCallback) {
        m_onNewWebsocketConnectionCallback(conn);
    }
}

bool WebSocketHandler::handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len) {
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.count(conn) == 0) {
        LOG_ERROR(subprocess) << "error in handleData, connections do not match";
        return false;
    }

    //LOG_INFO(subprocess) << "WS got " << data_len << "bytes";

    if (data_len < 1) {
        return true;
    }
    if (data_len == 2) {
        printf("%x %x\n", (int)data[0], (int)data[1]);
        return true;
    }
    

    //const std::string dataStr(data, data_len); // data is non-null-terminated c_str
    //LOG_INFO(subprocess) << dataStr;
    //if (boost::starts_with(dataStr, "CONNECT"))
    //{ // send an initial packet from behind the windows firewall to the server
    //}

    if (m_onNewWebsocketDataReceivedCallback) {
        return m_onNewWebsocketDataReceivedCallback(conn, data, data_len);
    }

    return true; // return true to keep socket open
}

void WebSocketHandler::handleClose(CivetServer *server, const struct mg_connection *conn) {
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.erase((struct mg_connection *)conn) == 0) {
        // if nothing was erased
        LOG_ERROR(subprocess) << "error in handleClose, connections do not match";
    }
    LOG_INFO(subprocess) << "WS closed";
}
*/
