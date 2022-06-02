/**
 * @file scheduler.h
 * @author Nadia Kortas <nadia.kortas@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This Scheduler class is a process which sends LINKUP or LINKDOWN events to Ingress and Storage modules to determine if a given bundle 
 * should be forwarded immediately to Egress or should be stored. 
 * To determine a given link availability, i.e. when a contact to a particular neighbor node exists, the scheduler reads a contact plan 
 * which is a JSON file that defines all the connections between all the nodes in the network.
 */


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
    int finalDest;
    int start;
    int end;
    int rate; 
};
typedef std::vector<contactPlan_t> contactPlanVector_t;

class Scheduler {
public:
    Scheduler();
    ~Scheduler();
    bool Run(int argc, const char* const argv[], volatile bool & running,
             std::string jsonEventFileName, bool useSignalHandler);
    int ProcessContactsFile(std::string* jsonEventFileName, zmq::socket_t * socket);
    int ProcessComandLine(int argc, const char *argv[],
                          std::string& jsonEventFileName);

    static std::string GetFullyQualifiedFilename(std::string filename) {
        return (Environment::GetPathHdtnSourceRoot() / "module/scheduler/src/").string() + filename;
    }

    volatile bool m_timersFinished;
    static const std::string DEFAULT_FILE;

    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundEgressToConnectingSchedulerPtr;
    std::unique_ptr<zmq::context_t> m_zmqCtxPtr;

    zmq::socket_t socket;

private:
    void SendLinkUp(int src, int dest, cbhe_eid_t finalDestinationEid,
                           zmq::socket_t * ptrSocket);
    void SendLinkDown(int src, int dest, cbhe_eid_t finalDestinationEid,
                           zmq::socket_t * ptrSocket);
    void ProcessLinkUp(const boost::system::error_code&, int src, int dest,
		                            cbhe_eid_t finalDestinationEid,
                                            std::string event, zmq::socket_t * ptrSocket);
    void ProcessLinkDown(const boost::system::error_code&, int src,
                         int dest, cbhe_eid_t finalDestinationEid,
			 std::string event, zmq::socket_t * ptrSocket);

    static void PingCommand(const boost::system::error_code& e, boost::asio::deadline_timer* t, 
		            const cbhe_eid_t* finalDestinationEid, zmq::socket_t * ptrSocket, const char* command);
     
    void MonitorExitKeypressThreadFunction();
    void EgressEventsHandler(zmq::socket_t * socket);
    //void ReadZmqAcksThreadFunc(volatile bool running, zmq::socket_t * ptrSocket);
    void ReadZmqAcksThreadFunc(volatile bool running, zmq::socket_t * socket);
    volatile bool m_runningFromSigHandler;
    HdtnConfig m_hdtnConfig;
    std::unique_ptr<boost::thread> m_threadZmqAckReaderPtr;
    std::vector<uint64_t> m_egressRxBufPtrToStdVec64;
};


#endif // SCHEDULER_H
