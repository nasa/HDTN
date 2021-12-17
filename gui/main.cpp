#include <boost/thread.hpp>
#include <boost/make_shared.hpp>
#include "WebsocketServer.h"
#include <iostream>
#include <boost/filesystem.hpp>

static volatile bool m_running = true;

void MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_running = false; //do this first
}

int main()
{
    const std::string DOCUMENT_ROOT = ".";
    const std::string HTML_FILE_NAME = "web_gui.html";
    const std::string PORT_NUMBER_AS_STRING = "8086";

    const boost::filesystem::path htmlMainFilePath = boost::filesystem::path(DOCUMENT_ROOT) / boost::filesystem::path(HTML_FILE_NAME);
    if (boost::filesystem::is_regular_file(htmlMainFilePath)) {
        std::cout << "found " << htmlMainFilePath.string() << std::endl;
    }
    else {
        std::cout << "Cannot find " << htmlMainFilePath.string() << " : make sure document_root is set properly in allconfig.xml" << std::endl;
        return 1;
    }
    std::cout << "starting websocket server\n";
    WebsocketServer server(DOCUMENT_ROOT, PORT_NUMBER_AS_STRING);

    while (m_running && !server.RequestsExit()) {

    }


    std::cout << "exiting\n";
    boost::this_thread::sleep(boost::posix_time::seconds(1));
    return 0;

}
