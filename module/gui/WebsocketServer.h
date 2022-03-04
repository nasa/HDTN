#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H 1

#include "CivetServer.h"
#include <cstring>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <set>
#include "hdtn_gui_export.h"
#ifndef CLASS_VISIBILITY_HDTN_GUI
#  ifdef _WIN32
#    define CLASS_VISIBILITY_HDTN_GUI
#  else
#    define CLASS_VISIBILITY_HDTN_GUI HDTN_GUI_EXPORT
#  endif
#endif

class CLASS_VISIBILITY_HDTN_GUI ExitHandler : public CivetHandler {
public:
    HDTN_GUI_EXPORT ExitHandler();
    HDTN_GUI_EXPORT bool handleGet(CivetServer *server, struct mg_connection *conn);

    volatile bool m_exitNow;
};

class CLASS_VISIBILITY_HDTN_GUI WebSocketHandler : public CivetWebSocketHandler {
public:
    HDTN_GUI_EXPORT WebSocketHandler();
    HDTN_GUI_EXPORT ~WebSocketHandler();
    HDTN_GUI_EXPORT void SendTextDataToActiveWebsockets(const char * data, std::size_t size);
    HDTN_GUI_EXPORT void SendBinaryDataToActiveWebsockets(const char * data, std::size_t size);

private:
    HDTN_GUI_NO_EXPORT virtual bool handleConnection(CivetServer *server, const struct mg_connection *conn);
    HDTN_GUI_NO_EXPORT virtual void handleReadyState(CivetServer *server, struct mg_connection *conn);
    HDTN_GUI_NO_EXPORT virtual bool handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len);
    HDTN_GUI_NO_EXPORT virtual void handleClose(CivetServer *server, const struct mg_connection *conn);

    boost::mutex m_mutex;
    //struct mg_connection * volatile m_activeConnection; //only allow one connection
    std::set<struct mg_connection *> m_activeConnections; //allow multiple connections
};

class CLASS_VISIBILITY_HDTN_GUI WebsocketServer {
public:
    HDTN_GUI_EXPORT WebsocketServer(const std::string & documentRoot, const std::string & portNumberAsString);
    HDTN_GUI_EXPORT bool RequestsExit();
    HDTN_GUI_EXPORT void SendNewBinaryData(const char* data, std::size_t size);
    HDTN_GUI_EXPORT void SendNewTextData(const char* data, std::size_t size);
    HDTN_GUI_EXPORT void SendNewTextData(const std::string & data);
    HDTN_GUI_EXPORT ~WebsocketServer();

private:
    WebsocketServer();

    boost::shared_ptr<CivetServer> m_civetServerSharedPtr;
    boost::shared_ptr<ExitHandler> m_exitHandlerSharedPtr;
    boost::shared_ptr<WebSocketHandler> m_websocketHandlerSharedPtr;
};


#endif
