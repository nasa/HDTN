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
#include <boost/make_unique.hpp>
#include "SignalHandler.h"
#include <boost/program_options.hpp>
#include "Environment.h"

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





WebSocketHandler::WebSocketHandler(zmq::context_t * hdtnOneProcessZmqInprocContextPtr) : CivetWebSocketHandler() {

    //start zmq thread
    m_running = true;
    m_threadZmqReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&WebSocketHandler::ReadZmqThreadFunc, this, hdtnOneProcessZmqInprocContextPtr)); //create and start the worker thread

}

WebSocketHandler::~WebSocketHandler() {
    m_running = false;
    if (m_threadZmqReaderPtr) {
        m_threadZmqReaderPtr->join();
        m_threadZmqReaderPtr.reset(); //delete it
    }
}

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

void WebSocketHandler::ReadZmqThreadFunc(zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {

    std::unique_ptr<zmq::context_t> zmqCtxPtr;
    //ingress
    std::unique_ptr<zmq::socket_t> zmqReqSock_connectingGuiToFromBoundIngressPtr;
    //egress
    std::unique_ptr<zmq::socket_t> zmqReqSock_connectingGuiToFromBoundEgressPtr;
    //storage
    std::unique_ptr<zmq::socket_t> zmqReqSock_connectingGuiToFromBoundStoragePtr;

    const uint8_t guiByteSignal = 1;
    const zmq::const_buffer guiByteSignalBuf(&guiByteSignal, sizeof(guiByteSignal));
    
    try {
        if (hdtnOneProcessZmqInprocContextPtr) {

            // socket for cut-through mode straight to egress
            //The io_threads argument specifies the size of the 0MQ thread pool to handle I/O operations.
            //If your application is using only the inproc transport for messaging you may set this to zero, otherwise set it to at least one. 
            zmqReqSock_connectingGuiToFromBoundIngressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            zmqReqSock_connectingGuiToFromBoundIngressPtr->connect(std::string("inproc://connecting_gui_to_from_bound_ingress"));

            zmqReqSock_connectingGuiToFromBoundEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            zmqReqSock_connectingGuiToFromBoundEgressPtr->connect(std::string("inproc://connecting_gui_to_from_bound_egress"));
            
            zmqReqSock_connectingGuiToFromBoundStoragePtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            zmqReqSock_connectingGuiToFromBoundStoragePtr->connect(std::string("inproc://connecting_gui_to_from_bound_storage"));
        }
        else {
            zmqCtxPtr = boost::make_unique<zmq::context_t>();
            //ingress
            zmqReqSock_connectingGuiToFromBoundIngressPtr = boost::make_unique<zmq::socket_t>(*zmqCtxPtr, zmq::socket_type::req);
            const std::string connect_connectingGuiToFromBoundIngressPath("tcp://localhost:10301");
            zmqReqSock_connectingGuiToFromBoundIngressPtr->connect(connect_connectingGuiToFromBoundIngressPath);
            
            //egress
            zmqReqSock_connectingGuiToFromBoundEgressPtr = boost::make_unique<zmq::socket_t>(*zmqCtxPtr, zmq::socket_type::req);
            const std::string connect_connectingGuiToFromBoundEgressPath("tcp://localhost:10302");
            zmqReqSock_connectingGuiToFromBoundEgressPtr->connect(connect_connectingGuiToFromBoundEgressPath);

            //storage
            zmqReqSock_connectingGuiToFromBoundStoragePtr = boost::make_unique<zmq::socket_t>(*zmqCtxPtr, zmq::socket_type::req);
            const std::string connect_connectingGuiToFromBoundStoragePath("tcp://localhost:10303");
            zmqReqSock_connectingGuiToFromBoundStoragePtr->connect(connect_connectingGuiToFromBoundStoragePath);
        }
    }
    catch (const zmq::error_t & ex) {
        std::cerr << "error: gui cannot connect zmq socket: " << ex.what() << std::endl;
        return;
    }

    //Caution: All options, with the exception of ZMQ_SUBSCRIBE, ZMQ_UNSUBSCRIBE and ZMQ_LINGER, only take effect for subsequent socket bind/connects.
    //The value of 0 specifies no linger period. Pending messages shall be discarded immediately when the socket is closed with zmq_close().
    zmqReqSock_connectingGuiToFromBoundIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
    zmqReqSock_connectingGuiToFromBoundEgressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
    zmqReqSock_connectingGuiToFromBoundStoragePtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr



    boost::asio::io_service ioService;
    boost::posix_time::time_duration sleepValTimeDuration = boost::posix_time::milliseconds(1000);
    boost::asio::deadline_timer deadlineTimer(ioService, sleepValTimeDuration);

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    static constexpr unsigned int NUM_SOCKETS = 3;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {zmqReqSock_connectingGuiToFromBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {zmqReqSock_connectingGuiToFromBoundEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {zmqReqSock_connectingGuiToFromBoundStoragePtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    
    while (m_running) {
        //sleep for 1 second
        boost::system::error_code ec;
        deadlineTimer.wait(ec);
        if (ec) {
            std::cout << "timer error: " << ec.message() << std::endl;
            return;
        }
        deadlineTimer.expires_at(deadlineTimer.expires_at() + sleepValTimeDuration);

        //std::cout << "loop\n";
        //send signals to all hdtn modules
        try {
            if (!zmqReqSock_connectingGuiToFromBoundIngressPtr->send(guiByteSignalBuf, zmq::send_flags::dontwait)) {
                std::cerr << "gui can't send signal to ingress" << std::endl;
            }
        }
        catch (zmq::error_t &) {
            std::cout << "request already sent to ingress\n";
        }

        try {
            if (!zmqReqSock_connectingGuiToFromBoundEgressPtr->send(guiByteSignalBuf, zmq::send_flags::dontwait)) {
                std::cerr << "gui can't send signal to egress" << std::endl;
            }
        }
        catch (zmq::error_t &) {
            std::cout << "request already sent to egress\n";
        }

        try {
            if (!zmqReqSock_connectingGuiToFromBoundStoragePtr->send(guiByteSignalBuf, zmq::send_flags::dontwait)) {
                std::cerr << "gui can't send signal to storage" << std::endl;
            }
        }
        catch (zmq::error_t &) {
            std::cout << "request already sent to storage\n";
        }

        //wait for telemetry from all modules
        unsigned int moduleMask = 0;
        for (unsigned int attempt = 0; attempt < 4; ++attempt) {
            if (moduleMask == 0x7) { //ingress=bit0, egress=bit1, storage=bit2
                break; //got all telemetry
            }
            int rc = 0; //return code
            try {
                rc = zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
            }
            catch (zmq::error_t & e) {
                std::cout << "caught zmq::error_t in WebSocketHandler::ReadZmqThreadFunc: " << e.what() << std::endl;
                continue;
            }
            if (rc > 0) {
                if (items[0].revents & ZMQ_POLLIN) { //ingress telemetry received
                    uint64_t telem;
                    const zmq::recv_buffer_result_t res = zmqReqSock_connectingGuiToFromBoundIngressPtr->recv(zmq::mutable_buffer(&telem, sizeof(telem)), zmq::recv_flags::dontwait);
                    if (!res) {
                        std::cerr << "error in WebSocketHandler::ReadZmqThreadFunc: cannot read ingress telemetry" << std::endl;
                    }
                    else if ((res->truncated()) || (res->size != sizeof(telem))) {
                        std::cerr << "ingress telemetry message mismatch: untruncated = " << res->untruncated_size
                            << " truncated = " << res->size << " expected = " << sizeof(telem) << std::endl;
                    }
                    else {
                        //process ingress telemetry
                        moduleMask |= 0x1;
                        std::cout << "ingress rx telem=" << telem << "\n";
                    }
                }
                if (items[1].revents & ZMQ_POLLIN) { //egress telemetry received
                    uint64_t telem;
                    const zmq::recv_buffer_result_t res = zmqReqSock_connectingGuiToFromBoundEgressPtr->recv(zmq::mutable_buffer(&telem, sizeof(telem)), zmq::recv_flags::dontwait);
                    if (!res) {
                        std::cerr << "error in WebSocketHandler::ReadZmqThreadFunc: cannot read egress telemetry" << std::endl;
                    }
                    else if ((res->truncated()) || (res->size != sizeof(telem))) {
                        std::cerr << "egress telemetry message mismatch: untruncated = " << res->untruncated_size
                            << " truncated = " << res->size << " expected = " << sizeof(telem) << std::endl;
                    }
                    else {
                        //process egress telemetry
                        moduleMask |= 0x2;
                        std::cout << "egress rx telem=" << telem << "\n";
                    }
                }
                if (items[2].revents & ZMQ_POLLIN) { //storage telemetry received
                    uint64_t telem;
                    const zmq::recv_buffer_result_t res = zmqReqSock_connectingGuiToFromBoundStoragePtr->recv(zmq::mutable_buffer(&telem, sizeof(telem)), zmq::recv_flags::dontwait);
                    if (!res) {
                        std::cerr << "error in WebSocketHandler::ReadZmqThreadFunc: cannot read storage telemetry" << std::endl;
                    }
                    else if ((res->truncated()) || (res->size != sizeof(telem))) {
                        std::cerr << "storage telemetry message mismatch: untruncated = " << res->untruncated_size
                            << " truncated = " << res->size << " expected = " << sizeof(telem) << std::endl;
                    }
                    else {
                        //process egress telemetry
                        moduleMask |= 0x4;
                        std::cout << "storage rx telem=" << telem << "\n";
                    }
                }
            }
        }

        //process all telemetry
        if (moduleMask != 0x7) {
            std::cout << "did not get telemetry from all modules\n";
        }
        else {
            //do websocket send of data from ingress, egress, and storage
        }
    }
    std::cout << "ReadZmqThreadFunc exiting\n";
    zmqReqSock_connectingGuiToFromBoundIngressPtr.reset();
    zmqReqSock_connectingGuiToFromBoundEgressPtr.reset();
    zmqReqSock_connectingGuiToFromBoundStoragePtr.reset();
    zmqCtxPtr.reset();
}



WebsocketServer::WebsocketServer() {}

void WebsocketServer::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}

bool WebsocketServer::Init(const std::string & documentRoot, const std::string & portNumberAsString, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {
    std::cout << "starting websocket server\n";
    const char *options[] = {
    "document_root", documentRoot.c_str(), "listening_ports", portNumberAsString.c_str(), 0 };

    std::vector<std::string> cpp_options;
    for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
        cpp_options.push_back(options[i]);
    }

    m_civetServerPtr = boost::make_unique<CivetServer>(cpp_options);
    m_exitHandlerPtr = boost::make_unique<ExitHandler>();
    m_websocketHandlerPtr = boost::make_unique<WebSocketHandler>(hdtnOneProcessZmqInprocContextPtr);

    m_civetServerPtr->addHandler(EXIT_URI, *m_exitHandlerPtr);
    m_civetServerPtr->addWebSocketHandler("/websocket", *m_websocketHandlerPtr);

    std::cout << "Run server at http://localhost:" << portNumberAsString << std::endl;
    std::cout << "Exit at http://localhost:" << portNumberAsString << EXIT_URI << std::endl;

    return true;
}

bool WebsocketServer::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&WebsocketServer::MonitorExitKeypressThreadFunction, this));

        std::string DOCUMENT_ROOT;
        const std::string HTML_FILE_NAME = "web_gui.html";
        std::string PORT_NUMBER_AS_STRING;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("document-root", boost::program_options::value<std::string>()->default_value((Environment::GetPathHdtnSourceRoot() / "module" / "gui" / "src" ).string()), "Document Root.")
                ("port-number", boost::program_options::value<uint16_t>()->default_value(8086), "Port number.")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return false;
            }

            DOCUMENT_ROOT = vm["document-root"].as<std::string>();
            const uint16_t portNumberAsUi16 = vm["port-number"].as<uint16_t>();
            PORT_NUMBER_AS_STRING = boost::lexical_cast<std::string>(portNumberAsUi16);

            const boost::filesystem::path htmlMainFilePath = boost::filesystem::path(DOCUMENT_ROOT) / boost::filesystem::path(HTML_FILE_NAME);
            if (boost::filesystem::is_regular_file(htmlMainFilePath)) {
                std::cout << "found " << htmlMainFilePath.string() << std::endl;
            }
            else {
                std::cout << "Cannot find " << htmlMainFilePath.string() << " : make sure document_root is set properly in allconfig.xml" << std::endl;
                return false;
            }
            

        }
        catch (boost::bad_any_cast & e) {
            std::cout << "invalid data error: " << e.what() << "\n\n";
            //hdtn::Logger::getInstance()->logError("egress", "Invalid data error: " + std::string(e.what()));
            std::cout << desc << "\n";
            return false;
        }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            //hdtn::Logger::getInstance()->logError("egress", "Error: " + std::string(e.what()));
            return false;
        }
        catch (...) {
            std::cerr << "Exception of unknown type!\n";
            //hdtn::Logger::getInstance()->logError("egress", "Exception of unknown type!");
            return false;
        }

        Init(DOCUMENT_ROOT, PORT_NUMBER_AS_STRING, NULL);
        

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "websocket server up and running" << std::endl;
        //hdtn::Logger::getInstance()->logNotification("egress", "Egress up and running.");
        while (running && m_runningFromSigHandler && !RequestsExit()) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        std::cout << "websocket server runner: exiting cleanly..\n";
        
    }
    std::cout << "websocket server runner: exited cleanly\n";
    //hdtn::Logger::getInstance()->logNotification("egress", "EgressAsyncRunner: Exited cleanly");
    return true;
}

bool WebsocketServer::RequestsExit() {
    return m_exitHandlerPtr->m_exitNow;
}

void WebsocketServer::SendNewBinaryData(const char* data, std::size_t size) {
    m_websocketHandlerPtr->SendBinaryDataToActiveWebsockets(data, size);
}

void WebsocketServer::SendNewTextData(const char* data, std::size_t size) {
    m_websocketHandlerPtr->SendTextDataToActiveWebsockets(data, size);
}

void WebsocketServer::SendNewTextData(const std::string & data) {
    m_websocketHandlerPtr->SendTextDataToActiveWebsockets(data.c_str(), data.size());
}

WebsocketServer::~WebsocketServer() {
    //m_civetServerSharedPtr->removeHandler(EXIT_URI);
    //m_civetServerSharedPtr->removeWebSocketHandler("/websocket");
    m_civetServerPtr.reset(); //delete main server before handlers to prevent crash
    m_exitHandlerPtr.reset();
    m_websocketHandlerPtr.reset();
}


