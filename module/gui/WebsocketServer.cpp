/* Copyright (c) 2013-2017 the Civetweb developers
 * Copyright (c) 2013 No Face Press, LLC
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

 // Simple example program on how to use Embedded C++ interface.
#include "WebsocketServer.h"
#include <boost/filesystem.hpp>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/make_shared.hpp>

#define EXIT_URI "/exit"
static const std::string CONNECT_MESSAGE = "hyxifwtd";

ExitHandler::ExitHandler() : CivetHandler(), m_exitNow(false) {}

bool ExitHandler::handleGet(CivetServer *server, struct mg_connection *conn) {
    mg_printf(conn,
        "HTTP/1.1 200 OK\r\nContent-Type: "
        "text/plain\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "Bye!\n");
    m_exitNow = true;
    return true;
};





WebSocketHandler::WebSocketHandler() : CivetWebSocketHandler() {}

WebSocketHandler::~WebSocketHandler() {}

void WebSocketHandler::SendTextDataToActiveWebsockets(const char * data, std::size_t size) {
    boost::mutex::scoped_lock lock(m_mutex);
    for (std::set<struct mg_connection *>::iterator it = m_activeConnections.begin(); it != m_activeConnections.end(); ++it) {
        mg_websocket_write(*it, MG_WEBSOCKET_OPCODE_TEXT, data, size);
    }
}

void WebSocketHandler::SendBinaryDataToActiveWebsockets(const char * data, std::size_t size) {
    boost::mutex::scoped_lock lock(m_mutex);
    for (std::set<struct mg_connection *>::iterator it = m_activeConnections.begin(); it != m_activeConnections.end(); ++it) {
        mg_websocket_write(*it, MG_WEBSOCKET_OPCODE_BINARY, data, size);
    }
}


bool WebSocketHandler::handleConnection(CivetServer *server, const struct mg_connection *conn) {
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.insert((struct mg_connection *) conn).second) {
        printf("WS connected\n");
        return true;
    }
    else {
        printf("ERROR, this WS is already connected\n");
        return false;
    }
}

void WebSocketHandler::handleReadyState(CivetServer *server, struct mg_connection *conn) {
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.count(conn) == 0) {
        std::cout << "error in handleReadyState, connections do not match";
        return;
    }
    printf("WS ready\n");

    const char *text = "Hello websocket";
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, text, strlen(text));
}

bool WebSocketHandler::handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len) {
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.count(conn) == 0) {
        std::cout << "error in handleData, connections do not match";
        return false;
    }

    printf("WS got %lu bytes\n", (long unsigned)data_len);

    if (data_len < 1) {
        return true;
    }



    const std::string dataStr(data, data_len); //data is non-null-terminated c_str
    std::cout << dataStr << "\n";
    if (boost::starts_with(dataStr, "CONNECT")) { //send an initial packet from behind the windows firewall to the server

    }

    return true; //return true to keep socket open
}

void WebSocketHandler::handleClose(CivetServer *server, const struct mg_connection *conn) {
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_activeConnections.erase((struct mg_connection *) conn) == 0) { //if nothing was erased
        std::cout << "error in handleClose, connections do not match";
    }
    printf("WS closed\n");
}



WebsocketServer::WebsocketServer(const std::string & documentRoot, const std::string & portNumberAsString) {
    const char *options[] = {
        "document_root", documentRoot.c_str(), "listening_ports", portNumberAsString.c_str(), 0 };

    std::vector<std::string> cpp_options;
    for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
        cpp_options.push_back(options[i]);
    }

    m_civetServerSharedPtr = boost::shared_ptr<CivetServer>(new CivetServer(cpp_options));
    m_exitHandlerSharedPtr = boost::shared_ptr<ExitHandler>(new ExitHandler());
    m_websocketHandlerSharedPtr = boost::shared_ptr<WebSocketHandler>(new WebSocketHandler());

    m_civetServerSharedPtr->addHandler(EXIT_URI, *m_exitHandlerSharedPtr);
    m_civetServerSharedPtr->addWebSocketHandler("/websocket", *m_websocketHandlerSharedPtr);

    std::cout << "Run server at http://localhost:" << portNumberAsString << std::endl;
    std::cout << "Exit at http://localhost:" << portNumberAsString << EXIT_URI << std::endl;

}

bool WebsocketServer::RequestsExit() {
    return m_exitHandlerSharedPtr->m_exitNow;
}

void WebsocketServer::SendNewBinaryData(const char* data, std::size_t size) {
    m_websocketHandlerSharedPtr->SendBinaryDataToActiveWebsockets(data, size);
}

void WebsocketServer::SendNewTextData(const char* data, std::size_t size) {
    m_websocketHandlerSharedPtr->SendTextDataToActiveWebsockets(data, size);
}

void WebsocketServer::SendNewTextData(const std::string & data) {
    m_websocketHandlerSharedPtr->SendTextDataToActiveWebsockets(data.c_str(), data.size());
}

WebsocketServer::~WebsocketServer() {
    //m_civetServerSharedPtr->removeHandler(EXIT_URI);
    //m_civetServerSharedPtr->removeWebSocketHandler("/websocket");
    m_civetServerSharedPtr = boost::shared_ptr<CivetServer>(); //delete main server before handlers to prevent crash
    m_exitHandlerSharedPtr = boost::shared_ptr<ExitHandler>();
    m_websocketHandlerSharedPtr = boost::shared_ptr<WebSocketHandler>();
}


