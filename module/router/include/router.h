#ifndef ROUTER_H
#define ROUTER_H

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time.hpp>
#include <boost/program_options.hpp>
#include <boost/make_unique.hpp>
#include <cstdlib>
#include <iostream>
#include "message.hpp"
#include "Environment.h"
#include "JsonSerializable.h"
#include "HdtnConfig.h"
#include "zmq.hpp"

typedef std::unique_ptr<boost::asio::deadline_timer> SmartDeadlineTimer;

class Router {
public:
    static const std::string DEFAULT_FILE;
    Router();
    ~Router();
    bool Run(int argc, const char* const argv[], volatile bool & running,
                    std::string jsonEventFileName, bool useSignalHandler);
    int ComputeOptimalRoute(std::string* jsonEventFileName,
                            uint64_t sourceNode, uint64_t finalDestNodeId);

    static std::string GetFullyQualifiedFilename(std::string filename) {
        return (Environment::GetPathHdtnSourceRoot() / "module/router/src/").string() + filename;
    }
    volatile bool m_timersFinished;
private:
    void RouteUpdate(const boost::system::error_code& e, uint64_t nextHopNodeId,
        uint64_t finalDestNodeId, std::string event, zmq::socket_t * ptrSocket);
    HdtnConfig m_hdtnConfig;
    void MonitorExitKeypressThreadFunction();
    volatile bool m_runningFromSigHandler;
};
#endif // ROUTER_H
