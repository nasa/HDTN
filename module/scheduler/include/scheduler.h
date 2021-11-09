#ifndef SCHEDULER_H
#define SCHEDULER_H

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
struct contactPlan_t {
    int contact;
    int source; 
    int dest; 
    int start;
    int end;
    int rate; 
};
typedef std::vector<contactPlan_t> contactPlanVector_t;

class Scheduler {
public:
    static const std::string DEFAULT_FILE;
    Scheduler();
    ~Scheduler();
    bool Run(int argc, const char* const argv[], volatile bool & running,
                    std::string jsonEventFileName, bool useSignalHandler);
    int ProcessContactsFile(std::string* jsonEventFileName, cbhe_eid_t finalDest);
    int ProcessComandLine(int argc, const char *argv[],
                          std::string& jsonEventFileName);

    static std::string GetFullyQualifiedFilename(std::string filename) {
        return (Environment::GetPathHdtnSourceRoot() / "module/scheduler/src/").string() + filename;
    }
    volatile bool m_timersFinished;
private:
    void ProcessLinkUp(const boost::system::error_code&, int src, int dest,
		       cbhe_eid_t finalDestinationEid,
                       std::string event, zmq::socket_t * ptrSocket);
    void ProcessLinkDown(const boost::system::error_code&, int src,
                         int dest, cbhe_eid_t finalDestinationEid,
			 std::string event, zmq::socket_t * ptrSocket);

    static void PingCommand(const boost::system::error_code& e, boost::asio::deadline_timer* t, 
		            const cbhe_eid_t* finalDestinationEid, zmq::socket_t * ptrSocket, const char* command);
     
    HdtnConfig m_hdtnConfig;
    void MonitorExitKeypressThreadFunction();
    volatile bool m_runningFromSigHandler;
};
#endif // SCHEDULER_H
