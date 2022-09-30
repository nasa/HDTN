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
#include <boost/bimap.hpp>
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

    bool operator<(const contactPlan_t& o) const; //operator < so it can be used as a map key
};

class Scheduler {
public:
    typedef std::pair<boost::posix_time::ptime, uint64_t> ptime_index_pair_t; //used in case of identical ptimes for starting events
    typedef std::pair<contactPlan_t, bool> contactplan_islinkup_pair_t; //second parameter isLinkUp
    typedef boost::bimap<ptime_index_pair_t, contactplan_islinkup_pair_t> ptime_to_contactplan_bimap_t;

    Scheduler();
    ~Scheduler();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    int ProcessContactsPtPtr(std::shared_ptr<boost::property_tree::ptree>& contactsPtPtr, bool useUnixTimestamps);
    int ProcessContacts(const boost::property_tree::ptree & pt, bool useUnixTimestamps);
    int ProcessContactsJsonText(char* jsonText, bool useUnixTimestamps);
    int ProcessContactsJsonText(const std::string& jsonText, bool useUnixTimestamps);
    int ProcessContactsFile(const std::string & jsonEventFileName, bool useUnixTimestamps);

    static std::string GetFullyQualifiedFilename(std::string filename) {
        return (Environment::GetPathHdtnSourceRoot() / "module/scheduler/src/").string() + filename;
    }

    static const std::string DEFAULT_FILE;

private:
    void Stop();
    void SendLinkUp(uint64_t src, uint64_t dest, uint64_t finalDestinationNodeId);
    void SendLinkDown(uint64_t src, uint64_t dest, uint64_t finalDestinationNodeId);
     
    void MonitorExitKeypressThreadFunction();
    void EgressEventsHandler();
    void UisEventsHandler();
    void ReadZmqAcksThreadFunc(volatile bool & running);
    void TryRestartContactPlanTimer();
    void OnContactPlan_TimerExpired(const boost::system::error_code& e);
    bool AddContact_NotThreadSafe(const contactPlan_t& contact);

private:
    volatile bool m_runningFromSigHandler;
    HdtnConfig m_hdtnConfig;
    std::unique_ptr<boost::thread> m_threadZmqAckReaderPtr;
    std::vector<uint64_t> m_egressRxBufPtrToStdVec64;

    std::unique_ptr<zmq::context_t> m_zmqCtxPtr;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundEgressToConnectingSchedulerPtr;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundUisToConnectingSchedulerPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPubSock_boundSchedulerToConnectingSubsPtr;
    boost::mutex m_mutexZmqPubSock;

    
    ptime_to_contactplan_bimap_t m_ptimeToContactPlanBimap;
    boost::asio::io_service m_ioService;
    boost::asio::deadline_timer m_contactPlanTimer;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    bool m_contactPlanTimerIsRunning;
    boost::posix_time::ptime m_epoch;
};


#endif // SCHEDULER_H
