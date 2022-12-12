#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H 1

#include "CivetServer.h"
#include <cstring>
#include <memory>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <set>
#include "gui_lib_export.h"
#include "zmq.hpp"

#ifndef CLASS_VISIBILITY_GUI_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_GUI_LIB
#  else
#    define CLASS_VISIBILITY_GUI_LIB GUI_LIB_EXPORT
#  endif
#endif

class CLASS_VISIBILITY_GUI_LIB ExitHandler : public CivetHandler {
public:
    GUI_LIB_EXPORT ExitHandler();
    GUI_LIB_EXPORT bool handleGet(CivetServer *server, struct mg_connection *conn);

    volatile bool m_exitNow;
};

class CLASS_VISIBILITY_GUI_LIB WebSocketHandler : public CivetWebSocketHandler {
public:
    WebSocketHandler() = delete;
    GUI_LIB_EXPORT WebSocketHandler(zmq::context_t * hdtnOneProcessZmqInprocContextPtr);
    GUI_LIB_EXPORT ~WebSocketHandler();
    GUI_LIB_EXPORT void SendTextDataToActiveWebsockets(const char * data, std::size_t size);
    GUI_LIB_EXPORT void SendBinaryDataToActiveWebsockets(const char * data, std::size_t size);

private:
    GUI_LIB_NO_EXPORT void ReadZmqThreadFunc(zmq::context_t * hdtnOneProcessZmqInprocContextPtr);
    GUI_LIB_NO_EXPORT virtual bool handleConnection(CivetServer *server, const struct mg_connection *conn);
    GUI_LIB_NO_EXPORT virtual void handleReadyState(CivetServer *server, struct mg_connection *conn);
    GUI_LIB_NO_EXPORT virtual bool handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len);
    GUI_LIB_NO_EXPORT virtual void handleClose(CivetServer *server, const struct mg_connection *conn);

    boost::mutex m_mutex;
    //struct mg_connection * volatile m_activeConnection; //only allow one connection
    std::set<struct mg_connection *> m_activeConnections; //allow multiple connections

    //zmq thread members
    std::unique_ptr<boost::thread> m_threadZmqReaderPtr;
    volatile bool m_running;
};

class CLASS_VISIBILITY_GUI_LIB WebsocketServer {
public:
    GUI_LIB_EXPORT WebsocketServer();
    GUI_LIB_EXPORT bool Init(const boost::filesystem::path& documentRoot, const std::string & portNumberAsString, zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);
    GUI_LIB_EXPORT bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    GUI_LIB_EXPORT bool RequestsExit();
    GUI_LIB_EXPORT void SendNewBinaryData(const char* data, std::size_t size);
    GUI_LIB_EXPORT void SendNewTextData(const char* data, std::size_t size);
    GUI_LIB_EXPORT void SendNewTextData(const std::string & data);
    GUI_LIB_EXPORT ~WebsocketServer();

private:
    GUI_LIB_NO_EXPORT void MonitorExitKeypressThreadFunction();

    std::unique_ptr<CivetServer> m_civetServerPtr;
    std::unique_ptr<ExitHandler> m_exitHandlerPtr;
    std::unique_ptr<WebSocketHandler> m_websocketHandlerPtr;

    volatile bool m_runningFromSigHandler;
};


#endif
