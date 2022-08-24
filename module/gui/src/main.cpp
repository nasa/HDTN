#include <boost/thread.hpp>
#include <memory>
#include "WebsocketServer.h"
#include "Environment.h"
#include <iostream>
#include <boost/filesystem.hpp>


int main(int argc, const char* argv[]) {
    {
        WebsocketServer server;
        volatile bool running;
        server.Run(argc, argv, running, true);
        std::cout << "Websocket Server Exiting\n";
    }
    return 0;
}
