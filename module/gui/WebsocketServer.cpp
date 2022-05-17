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





WebSocketHandler::WebSocketHandler() : CivetWebSocketHandler() {

    //start zmq thread
    m_running = true;
    m_threadZmqReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&WebSocketHandler::ReadZmqThreadFunc, this)); //create and start the worker thread

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

void WebSocketHandler::ReadZmqThreadFunc() {

    while (m_running) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        std::cout << "loop\n";
    }
    std::cout << "ReadZmqThreadFunc exiting\n";
}



WebsocketServer::WebsocketServer() {}

void WebsocketServer::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
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
                ("document-root", boost::program_options::value<std::string>()->default_value((Environment::GetPathHdtnSourceRoot() / "module" / "gui").string()), "Document Root.")
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


        std::cout << "starting websocket server\n";
        const char *options[] = {
        "document_root", DOCUMENT_ROOT.c_str(), "listening_ports", PORT_NUMBER_AS_STRING.c_str(), 0 };

        std::vector<std::string> cpp_options;
        for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
            cpp_options.push_back(options[i]);
        }

        m_civetServerSharedPtr = boost::shared_ptr<CivetServer>(new CivetServer(cpp_options));
        m_exitHandlerSharedPtr = boost::shared_ptr<ExitHandler>(new ExitHandler());
        m_websocketHandlerSharedPtr = boost::shared_ptr<WebSocketHandler>(new WebSocketHandler());

        m_civetServerSharedPtr->addHandler(EXIT_URI, *m_exitHandlerSharedPtr);
        m_civetServerSharedPtr->addWebSocketHandler("/websocket", *m_websocketHandlerSharedPtr);

        std::cout << "Run server at http://localhost:" << PORT_NUMBER_AS_STRING << std::endl;
        std::cout << "Exit at http://localhost:" << PORT_NUMBER_AS_STRING << EXIT_URI << std::endl;

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


