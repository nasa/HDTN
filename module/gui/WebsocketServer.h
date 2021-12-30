#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H 1

#include "CivetServer.h"
#include <cstring>

#include <boost/thread.hpp> 
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <set>

class ExitHandler : public CivetHandler {
public:
	ExitHandler();
	bool handleGet(CivetServer *server, struct mg_connection *conn);

	volatile bool m_exitNow;
};

class WebSocketHandler : public CivetWebSocketHandler {
public:
	WebSocketHandler();
	~WebSocketHandler();
	void SendDataToActiveWebsockets(const char * data, std::size_t size);
	void SendBinaryDataToActiveWebsockets(const char * data, std::size_t size);

private:
	virtual bool handleConnection(CivetServer *server, const struct mg_connection *conn);
	virtual void handleReadyState(CivetServer *server, struct mg_connection *conn);
	virtual bool handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len);
	virtual void handleClose(CivetServer *server, const struct mg_connection *conn);
	void StartUdpReceive();
	void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred);
	void HandleUdpSend(const boost::system::error_code & error, std::size_t bytesTransferred);

	boost::mutex m_mutex;
	//struct mg_connection * volatile m_activeConnection; //only allow one connection
	std::set<struct mg_connection *> m_activeConnections; //allow multiple connections

	boost::asio::io_service m_ioService;
	boost::asio::ip::udp::socket m_udpSocket;
	boost::asio::ip::udp::endpoint m_endpointToApcFromVm;
	boost::asio::ip::udp::endpoint m_junkRemoteEndpointFromApcToVm;
	boost::shared_ptr<boost::thread> m_ioServiceThread;
	boost::uint8_t m_udpReceiveBuffer[2048];
};

class WebsocketServer {
public:
	WebsocketServer(const std::string & documentRoot, const std::string & portNumberAsString);
	bool RequestsExit();
	void SendNewTextData(const char* data, std::size_t size);
	void SendNewTextData(const std::string & data);
	~WebsocketServer();

private:
	WebsocketServer();

	boost::shared_ptr<CivetServer> m_civetServerSharedPtr;
	boost::shared_ptr<ExitHandler> m_exitHandlerSharedPtr;
	boost::shared_ptr<WebSocketHandler> m_websocketHandlerSharedPtr;	
};


#endif
