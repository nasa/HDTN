#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/make_unique.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include "WebsocketServer.h"
#include "Environment.h"
#include "Logger.h"
#include "SignalHandler.h"
#include "StatsLogger.h"
#include "TelemetryDefinitions.h"

#define EXIT_URI "/exit"
static const std::string CONNECT_MESSAGE = "hyxifwtd";
static const hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::gui;

ExitHandler::ExitHandler() : CivetHandler(), m_exitNow(false) {}

bool ExitHandler::handleGet(CivetServer *server, struct mg_connection *conn)
{
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "Bye!\n");
    m_exitNow = true;
    return true;
};

WebSocketHandler::WebSocketHandler() : CivetWebSocketHandler() {}

WebSocketHandler::~WebSocketHandler() {}

void WebSocketHandler::SendBinaryDataToActiveWebsockets(const char *data, std::size_t size)
{
    boost::mutex::scoped_lock lock(m_mutex);
    for (std::set<struct mg_connection *>::iterator it = m_activeConnections.begin(); it != m_activeConnections.end(); ++it)
    {
        mg_websocket_write(*it, MG_WEBSOCKET_OPCODE_BINARY, data, size);
    }
}

bool WebSocketHandler::handleConnection(CivetServer *server, const struct mg_connection *conn)
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.insert((struct mg_connection *)conn).second)
    {
        LOG_INFO(subprocess) << "WS connected";
        return true;
    }
    else
    {
        LOG_ERROR(subprocess) << "this WS is already connected";
        return false;
    }
}

void WebSocketHandler::handleReadyState(CivetServer *server, struct mg_connection *conn)
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.count(conn) == 0)
    {
        LOG_ERROR(subprocess) << "error in handleReadyState, connections do not match";
        return;
    }
    LOG_INFO(subprocess) << "WS ready";

    const char *text = "Hello websocket";
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, text, strlen(text));
}

bool WebSocketHandler::handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len)
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.count(conn) == 0)
    {
        LOG_ERROR(subprocess) << "error in handleData, connections do not match";
        return false;
    }

    LOG_INFO(subprocess) << "WS got" << data_len << "bytes";

    if (data_len < 1)
    {
        return true;
    }

    const std::string dataStr(data, data_len); // data is non-null-terminated c_str
    LOG_INFO(subprocess) << dataStr;
    if (boost::starts_with(dataStr, "CONNECT"))
    { // send an initial packet from behind the windows firewall to the server
    }

    return true; // return true to keep socket open
}

void WebSocketHandler::handleClose(CivetServer *server, const struct mg_connection *conn)
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.erase((struct mg_connection *)conn) == 0)
    { // if nothing was erased
        LOG_ERROR(subprocess) << "error in handleClose, connections do not match";
    }
    LOG_INFO(subprocess) << "WS closed";
}

WebsocketServer::WebsocketServer() {}

bool WebsocketServer::Init(const boost::filesystem::path& documentRoot, const std::string& portNumberAsString)
{
    const std::string documentRootAsString = documentRoot.string();
    LOG_INFO(subprocess) << "starting websocket server\n";
    const char *options[] = {
        "document_root", documentRootAsString.c_str(), "listening_ports", portNumberAsString.c_str(), 0};

    std::vector<std::string> cpp_options;
    for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++)
    {
        cpp_options.push_back(options[i]);
    }

    m_civetServerPtr = boost::make_unique<CivetServer>(cpp_options);
    m_exitHandlerPtr = boost::make_unique<ExitHandler>();
    m_websocketHandlerPtr = boost::make_unique<WebSocketHandler>();

    m_civetServerPtr->addHandler(EXIT_URI, *m_exitHandlerPtr);
    m_civetServerPtr->addWebSocketHandler("/websocket", *m_websocketHandlerPtr);

    LOG_INFO(subprocess) << "Run server at http://localhost:" << portNumberAsString;
    LOG_INFO(subprocess) << "Exit at http://localhost:" << portNumberAsString;

    return true;
}

bool WebsocketServer::RequestsExit()
{
    return m_exitHandlerPtr->m_exitNow;
}

void WebsocketServer::SendNewBinaryData(const char *data, std::size_t size)
{
    m_websocketHandlerPtr->SendBinaryDataToActiveWebsockets(data, size);
}

WebsocketServer::~WebsocketServer()
{
    m_civetServerPtr.reset(); // delete main server before handlers to prevent crash
    m_exitHandlerPtr.reset();
    m_websocketHandlerPtr.reset();
}
