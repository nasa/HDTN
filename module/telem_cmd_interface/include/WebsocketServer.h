/**
 * @file WebsocketServer.h
 *
 * @copyright Copyright © 2022 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 * This WebsocketServer class implements a websocket server and client application for
 * displaying telemetry metrics in a graphical user interface
 */

#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H 1

#include <cstring>
#include <memory>
#include <set>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include "zmq.hpp"

#include "CivetServer.h"
#include "telem_lib_export.h"

#ifndef CLASS_VISIBILITY_GUI_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_GUI_LIB
#  else
#    define CLASS_VISIBILITY_GUI_LIB TELEM_LIB_EXPORT
#  endif
#endif

class CLASS_VISIBILITY_GUI_LIB ExitHandler : public CivetHandler {
public:
    TELEM_LIB_EXPORT ExitHandler();
    TELEM_LIB_EXPORT bool handleGet(CivetServer *server, struct mg_connection *conn);

    volatile bool m_exitNow;
};

class CLASS_VISIBILITY_GUI_LIB WebSocketHandler : public CivetWebSocketHandler {
public:
    TELEM_LIB_EXPORT WebSocketHandler();
    TELEM_LIB_EXPORT ~WebSocketHandler();
    TELEM_LIB_EXPORT void SendTextDataToActiveWebsockets(const char * data, std::size_t size);
    TELEM_LIB_EXPORT void SendBinaryDataToActiveWebsockets(const char * data, std::size_t size);

private:
    TELEM_LIB_NO_EXPORT virtual bool handleConnection(CivetServer *server, const struct mg_connection *conn);
    TELEM_LIB_NO_EXPORT virtual void handleReadyState(CivetServer *server, struct mg_connection *conn);
    TELEM_LIB_NO_EXPORT virtual bool handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len);
    TELEM_LIB_NO_EXPORT virtual void handleClose(CivetServer *server, const struct mg_connection *conn);

    boost::mutex m_mutex;
    std::set<struct mg_connection *> m_activeConnections; //allow multiple connections
};

class CLASS_VISIBILITY_GUI_LIB WebsocketServer {
public:
    TELEM_LIB_EXPORT WebsocketServer();
    TELEM_LIB_EXPORT bool Init(const boost::filesystem::path& documentRoot, const std::string& portNumberAsString);
    TELEM_LIB_EXPORT bool RequestsExit();
    TELEM_LIB_EXPORT void SendNewBinaryData(const char* data, std::size_t size);
    TELEM_LIB_EXPORT ~WebsocketServer();

private:
    std::unique_ptr<CivetServer> m_civetServerPtr;
    std::unique_ptr<ExitHandler> m_exitHandlerPtr;
    std::unique_ptr<WebSocketHandler> m_websocketHandlerPtr;
};


#endif
