/**
 * @file WebsocketServer.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Ethan Schweinsberg <ethan.e.schweinsberg@nasa.gov>
 *
 * @copyright Copyright (c) 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 */


#include "Logger.h"
#if defined(WEB_INTERFACE_USE_BEAST)
# include "BeastWebsocketServer.h"
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
# include "CivetwebWebsocketServer.h"
#endif
#include "WebsocketServer.h"
#include "Environment.h"
#include <boost/filesystem/operations.hpp>
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::telem;


/////////////////////////////
// PROGRAM OPTIONS
/////////////////////////////
static const boost::filesystem::path GUI_HTML_FILE_NAME = "index.html";


WebsocketServer::ProgramOptions::ProgramOptions()
    : m_guiDocumentRoot(""), m_guiPortNumber("")
{
}

#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
static boost::filesystem::path GetDocumentRootAndValidate(boost::program_options::variables_map& vm) {
    try {
        const boost::program_options::variable_value val = vm["document-root"];
        if (val.empty()) {
            return "";
        }
        const boost::filesystem::path documentRoot = val.as<boost::filesystem::path>();
        const boost::filesystem::path htmlMainFilePath = documentRoot / GUI_HTML_FILE_NAME;
        if (boost::filesystem::is_regular_file(htmlMainFilePath)) {
            return documentRoot;
        }
        else {
            LOG_FATAL(subprocess) << "Cannot find " << htmlMainFilePath << " : make sure document-root is set properly";
        }
    }
    catch (boost::bad_any_cast& e) {
        LOG_FATAL(subprocess) << "invalid program option error: " << e.what();
    }
    return "";
}

static std::string GetPortNumberAsString(boost::program_options::variables_map& vm) {
    try {
        const boost::program_options::variable_value val = vm["port-number"];
        if (val.empty()) {
            return "";
        }
        const uint16_t portNumber = val.as<uint16_t>();
        return boost::lexical_cast<std::string>(portNumber);
    }
    catch (boost::bad_any_cast& e) {
        LOG_FATAL(subprocess) << "invalid program options error: " << e.what();
        return "";
    }
}
#endif

#ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
static void GetSslPathsAndValidate(boost::program_options::variables_map& vm, WebsocketServer::ProgramOptions::SslPaths& sslPaths) {
    sslPaths.m_valid = false;
    try {
        sslPaths.m_certificatePemFile = vm["gui-certificate-pem-file"].as<boost::filesystem::path>();
        sslPaths.m_certificateChainPemFile = vm["gui-certificate-chain-pem-file"].as<boost::filesystem::path>();
        sslPaths.m_privateKeyPemFile = vm["gui-private-key-pem-file"].as<boost::filesystem::path>();
        sslPaths.m_diffieHellmanParametersPemFile = vm["gui-dh-pem-file"].as<boost::filesystem::path>();
        if (sslPaths.m_certificatePemFile.size() && sslPaths.m_certificateChainPemFile.size()) {
            LOG_FATAL(subprocess) << "gui-certificate-pem-file and gui-certificate-chain-pem-file cannot both be specified";
        }
        else {
            sslPaths.m_valid = (sslPaths.m_certificatePemFile.size() || sslPaths.m_certificateChainPemFile.size())
                && sslPaths.m_privateKeyPemFile.size()
                && sslPaths.m_diffieHellmanParametersPemFile.size();
        }
    }
    catch (boost::bad_any_cast& e) {
        LOG_FATAL(subprocess) << "invalid program option error: " << e.what();
    }
}
#endif

bool WebsocketServer::ProgramOptions::ParseFromVariableMap(boost::program_options::variables_map& vm) {
#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
    m_guiDocumentRoot = GetDocumentRootAndValidate(vm);
    m_guiPortNumber = GetPortNumberAsString(vm);
    if (m_guiDocumentRoot == "" || m_guiPortNumber == "") {
        return false;
    }
# ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
    GetSslPathsAndValidate(vm, m_sslPaths);
# endif
#else
    (void)vm;
#endif
    return true;
}

void WebsocketServer::ProgramOptions::AppendToDesc(boost::program_options::options_description& desc, const boost::filesystem::path* defaultWwwRoot)
{
#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
    desc.add_options()
        ("document-root", boost::program_options::value<boost::filesystem::path>()->default_value((defaultWwwRoot) ? *defaultWwwRoot : Environment::GetPathGuiDocumentRoot()), "Web GUI Document Root.")
        ("port-number", boost::program_options::value<uint16_t>()->default_value(8086), "Web GUI Port number.")
# ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
        ("gui-certificate-pem-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "GUI Server certificate file in PEM format (not preferred)")
        ("gui-certificate-chain-pem-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "GUI Server certificate chain file in PEM format (preferred)")
        ("gui-private-key-pem-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "GUI Server certificate file in PEM format")
        ("gui-dh-pem-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "GUI Server Diffie Hellman parameters file in PEM format")
# endif
        ;
#else
    (void)desc; //unused parameter
    (void)defaultWwwRoot; //unused parameter
#endif
}



/////////////////////////////
// CONNECTION
/////////////////////////////

void WebsocketServer::Connection::SendTextDataToThisConnection(std::shared_ptr<std::string>&& stringPtr) {
#if defined(WEB_INTERFACE_USE_BEAST)
    WebsocketSessionPublicBase* conn = (WebsocketSessionPublicBase*)m_pimpl;
    conn->AsyncSendTextData(std::move(stringPtr));
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    struct mg_connection* conn = (struct mg_connection*)m_pimpl;
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, stringPtr->data(), stringPtr->size());
#else
    (void)stringPtr;
#endif
}

void WebsocketServer::Connection::SendTextDataToThisConnection(const char* str, std::size_t strSize) {
#if defined(WEB_INTERFACE_USE_BEAST)
    WebsocketSessionPublicBase* conn = (WebsocketSessionPublicBase*)m_pimpl;
    conn->AsyncSendTextData(std::make_shared<std::string>(str, str + strSize));
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    struct mg_connection* conn = (struct mg_connection*)m_pimpl;
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, str, strSize);
#else
    (void)str;
    (void)strSize;
#endif
}

void WebsocketServer::Connection::SendTextDataToThisConnection(std::string&& str) {
#if defined(WEB_INTERFACE_USE_BEAST)
    WebsocketSessionPublicBase* conn = (WebsocketSessionPublicBase*)m_pimpl;
    conn->AsyncSendTextData(std::make_shared<std::string>(std::move(str)));
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    struct mg_connection* conn = (struct mg_connection*)m_pimpl;
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, str.data(), str.size());
#else
    (void)str;
#endif
}

void WebsocketServer::Connection::SendTextDataToThisConnection(const std::string& str) {
#if defined(WEB_INTERFACE_USE_BEAST)
    WebsocketSessionPublicBase* conn = (WebsocketSessionPublicBase*)m_pimpl;
    conn->AsyncSendTextData(std::make_shared<std::string>(str));
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    struct mg_connection* conn = (struct mg_connection*)m_pimpl;
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, str.data(), str.size());
#else
    (void)str;
#endif
}

void WebsocketServer::Connection::SendTextDataToThisConnection(const std::shared_ptr<std::string>& strPtr) {
#if defined(WEB_INTERFACE_USE_BEAST)
    WebsocketSessionPublicBase* conn = (WebsocketSessionPublicBase*)m_pimpl;
    conn->AsyncSendTextData(std::shared_ptr<std::string>(strPtr));
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    struct mg_connection* conn = (struct mg_connection*)m_pimpl;
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, strPtr->data(), strPtr->size());
#else
    (void)strPtr;
#endif
}

/////////////////////////////
// WEBSOCKET SERVER
/////////////////////////////

class WebsocketServer::Impl : private boost::noncopyable {
public:
    Impl();
    bool Init(const ProgramOptions& options,
        const OnNewWebsocketConnectionCallback_t& onNewWebsocketConnectionCallback,
        const OnNewWebsocketTextDataReceivedCallback_t& onNewWebsocketTextDataReceivedCallback);
    void Stop();

    void SendTextDataToActiveWebsockets(const char* str, std::size_t strSize);
    void SendTextDataToActiveWebsockets(std::string&& str);
    void SendTextDataToActiveWebsockets(const std::string& str);
    void SendTextDataToActiveWebsockets(const std::shared_ptr<std::string>& strPtr);

private:
#if defined(WEB_INTERFACE_USE_BEAST)
    void OnNewWebsocketConnectionCallback(WebsocketSessionPublicBase& conn);
    bool OnNewWebsocketDataReceivedCallback(WebsocketSessionPublicBase& conn, std::string& receivedString);
    std::unique_ptr<BeastWebsocketServer> m_websocketServerPtr;
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    void OnNewWebsocketConnectionCallback(struct mg_connection* conn);
    bool OnNewWebsocketDataReceivedCallback(struct mg_connection* conn, char* data, size_t data_len);
    std::unique_ptr<CivetwebWebsocketServer> m_websocketServerPtr;
#endif
    OnNewWebsocketConnectionCallback_t m_onNewWebsocketConnectionCallback;
    OnNewWebsocketTextDataReceivedCallback_t m_onNewWebsocketTextDataReceivedCallback;
};

WebsocketServer::WebsocketServer() :
    m_pimpl(boost::make_unique<WebsocketServer::Impl>()),
    m_valid(false)
{}

bool WebsocketServer::Init(const ProgramOptions& options,
    const OnNewWebsocketConnectionCallback_t& onNewWebsocketConnectionCallback,
    const OnNewWebsocketTextDataReceivedCallback_t& onNewWebsocketTextDataReceivedCallback)
{
    m_valid = m_pimpl->Init(options, onNewWebsocketConnectionCallback, onNewWebsocketTextDataReceivedCallback);
    return m_valid;
}

void WebsocketServer::Stop() {
    m_pimpl->Stop();
}

WebsocketServer::~WebsocketServer() {
    Stop();
}

bool WebsocketServer::EnabledAndValid() const noexcept {
#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
    return m_valid;
#else
    return false;
#endif
}

bool WebsocketServer::IsCompiled() noexcept {
#if defined(WEB_INTERFACE_USE_BEAST) || defined(WEB_INTERFACE_USE_CIVETWEB)
    return true;
#else
    return false;
#endif
}

bool WebsocketServer::IsCompiledWithSsl() noexcept {
#if defined(BEAST_WEBSOCKET_SERVER_SUPPORT_SSL)
    return true;
#else
    return false;
#endif
}

void WebsocketServer::SendTextDataToActiveWebsockets(const char* str, std::size_t strSize) {
    m_pimpl->SendTextDataToActiveWebsockets(str, strSize);
}
void WebsocketServer::SendTextDataToActiveWebsockets(std::string&& str) {
    m_pimpl->SendTextDataToActiveWebsockets(std::move(str));
}
void WebsocketServer::SendTextDataToActiveWebsockets(const std::string& str) {
    m_pimpl->SendTextDataToActiveWebsockets(str);
}
void WebsocketServer::SendTextDataToActiveWebsockets(const std::shared_ptr<std::string>& strPtr) {
    m_pimpl->SendTextDataToActiveWebsockets(strPtr);
}

/**
 * TelemetryRunner implementation
 */

WebsocketServer::Impl::Impl() {}

bool WebsocketServer::Impl::Init(const ProgramOptions& options,
    const OnNewWebsocketConnectionCallback_t& onNewWebsocketConnectionCallback,
    const OnNewWebsocketTextDataReceivedCallback_t& onNewWebsocketTextDataReceivedCallback)
{
    m_onNewWebsocketConnectionCallback = onNewWebsocketConnectionCallback;
    m_onNewWebsocketTextDataReceivedCallback = onNewWebsocketTextDataReceivedCallback;

#if defined(WEB_INTERFACE_USE_CIVETWEB)
    m_websocketServerPtr = boost::make_unique<CivetwebWebsocketServer>();
    if (!m_websocketServerPtr->Init(options.m_guiDocumentRoot, options.m_guiPortNumber)) {
        LOG_FATAL(subprocess) << "Cannot init CivetWeb websocket server";
        return false;
    }
    m_websocketServerPtr->SetOnNewWebsocketConnectionCallback(
        boost::bind(&WebsocketServer::Impl::OnNewWebsocketConnectionCallback, this, boost::placeholders::_1));
    m_websocketServerPtr->SetOnNewWebsocketDataReceivedCallback(
        boost::bind(&WebsocketServer::Impl::OnNewWebsocketDataReceivedCallback, this,
            boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
#elif defined(WEB_INTERFACE_USE_BEAST)
# ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
    boost::asio::ssl::context sslContext(boost::asio::ssl::context::sslv23_server);
    if (options.m_sslPaths.m_valid) {
        try {
            // tcpclv4 server supports tls 1.2 and 1.3 only
            sslContext.set_options(
                boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1 | boost::asio::ssl::context::single_dh_use);
            if (options.m_sslPaths.m_certificateChainPemFile.size()) {
                sslContext.use_certificate_chain_file(options.m_sslPaths.m_certificateChainPemFile.string());
            }
            else {
                sslContext.use_certificate_file(options.m_sslPaths.m_certificatePemFile.string(), boost::asio::ssl::context::pem);
            }
            sslContext.use_private_key_file(options.m_sslPaths.m_privateKeyPemFile.string(), boost::asio::ssl::context::pem);
            sslContext.use_tmp_dh_file(options.m_sslPaths.m_diffieHellmanParametersPemFile.string()); //"C:/hdtn_ssl_certificates/dh4096.pem"
        }
        catch (boost::system::system_error &e) {
            LOG_FATAL(subprocess) << "SSL error when trying to init Boost Beast websocket server: " << e.what();
            return false;
        }
    }
    m_websocketServerPtr = boost::make_unique<BeastWebsocketServer>(std::move(sslContext), options.m_sslPaths.m_valid);
# else
    m_websocketServerPtr = boost::make_unique<BeastWebsocketServer>();
# endif // #ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
    if (!m_websocketServerPtr->Init(options.m_guiDocumentRoot, options.m_guiPortNumber,
        boost::bind(&WebsocketServer::Impl::OnNewWebsocketConnectionCallback, this, boost::placeholders::_1),
        boost::bind(&WebsocketServer::Impl::OnNewWebsocketDataReceivedCallback, this, boost::placeholders::_1, boost::placeholders::_2)))
    {
        LOG_FATAL(subprocess) << "Cannot init Boost Beast websocket server";
        return false;
    }
#else //no web server support
    (void)options;
#endif // defined(WEB_INTERFACE_USE_BEAST)

    return true;
}

#if defined(WEB_INTERFACE_USE_BEAST)
void WebsocketServer::Impl::OnNewWebsocketConnectionCallback(WebsocketSessionPublicBase& conn) {
    Connection connWrapper;
    connWrapper.m_pimpl = &conn;
    m_onNewWebsocketConnectionCallback(connWrapper);
}

bool WebsocketServer::Impl::OnNewWebsocketDataReceivedCallback(WebsocketSessionPublicBase& conn, std::string& receivedString) {
    Connection connWrapper;
    connWrapper.m_pimpl = &conn;
    return m_onNewWebsocketTextDataReceivedCallback(connWrapper, receivedString);
}
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
void WebsocketServer::Impl::OnNewWebsocketConnectionCallback(struct mg_connection* conn) {
    Connection connWrapper;
    connWrapper.m_pimpl = conn;
    m_onNewWebsocketConnectionCallback(connWrapper);
}

bool WebsocketServer::Impl::OnNewWebsocketDataReceivedCallback(struct mg_connection* conn, char* data, size_t data_len) {
    Connection connWrapper;
    connWrapper.m_pimpl = conn;
    std::string receivedString(data, data + data_len);
    return m_onNewWebsocketTextDataReceivedCallback(connWrapper, receivedString);
}
#endif

void WebsocketServer::Impl::SendTextDataToActiveWebsockets(const char* str, std::size_t strSize) {
#if defined(WEB_INTERFACE_USE_BEAST)
    if (m_websocketServerPtr) {
        const std::shared_ptr<std::string> strPtr = std::make_shared<std::string>(str, str + strSize);
        m_websocketServerPtr->SendTextDataToActiveWebsockets(strPtr);
    }
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    if (m_websocketServerPtr) {
        m_websocketServerPtr->SendNewTextData(str, strSize);
    }
#else
    (void)str;
    (void)strSize;
#endif
}

void WebsocketServer::Impl::SendTextDataToActiveWebsockets(std::string&& str) {
#if defined(WEB_INTERFACE_USE_BEAST)
    if (m_websocketServerPtr) {
        const std::shared_ptr<std::string> strPtr = std::make_shared<std::string>(std::move(str));
        m_websocketServerPtr->SendTextDataToActiveWebsockets(strPtr);
    }
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    if (m_websocketServerPtr) {
        m_websocketServerPtr->SendNewTextData(str.data(), str.size());
    }
#else
    (void)str;
#endif
}

void WebsocketServer::Impl::SendTextDataToActiveWebsockets(const std::string& str) {
#if defined(WEB_INTERFACE_USE_BEAST)
    if (m_websocketServerPtr) {
        const std::shared_ptr<std::string> strPtr = std::make_shared<std::string>(str);
        m_websocketServerPtr->SendTextDataToActiveWebsockets(strPtr);
    }
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    if (m_websocketServerPtr) {
        m_websocketServerPtr->SendNewTextData(str.data(), str.size());
    }
#else
    (void)str;
#endif
}

void WebsocketServer::Impl::SendTextDataToActiveWebsockets(const std::shared_ptr<std::string>& strPtr) {
#if defined(WEB_INTERFACE_USE_BEAST)
    if (m_websocketServerPtr) {
        m_websocketServerPtr->SendTextDataToActiveWebsockets(strPtr);
    }
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    if (m_websocketServerPtr) {
        m_websocketServerPtr->SendNewTextData(strPtr->data(), strPtr->size());
    }
#else
    (void)strPtr;
#endif
}


void WebsocketServer::Impl::Stop() {
#if defined(WEB_INTERFACE_USE_BEAST)
    // stop websocket after thread
    if (m_websocketServerPtr) {
        m_websocketServerPtr->Stop();
        m_websocketServerPtr.reset();
    }
#elif defined(WEB_INTERFACE_USE_CIVETWEB)
    // stop websocket after thread
    if (m_websocketServerPtr) {
        m_websocketServerPtr.reset();
    }
#endif
}
