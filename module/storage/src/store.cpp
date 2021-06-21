/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
*/

#include "store.hpp"
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include <iostream>
#include "message.hpp"
#include "Logger.h"

#define HDTN_STORAGE_RECV_MODE "push"

hdtn::storage::storage() {

}
hdtn::storage::~storage() {
    Stop();
}
void hdtn::storage::Stop() {
    worker.Stop();
    m_totalBundlesErasedFromStorage = worker.m_totalBundlesErasedFromStorage;
    m_totalBundlesSentToEgressFromStorage = worker.m_totalBundlesSentToEgressFromStorage;
}

bool hdtn::storage::Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {
    m_hdtnConfig = hdtnConfig;


    std::cout << "[storage] Executing registration ..." << std::endl;
    hdtn::Logger::getInstance()->logNotification("storage", "Executing Registration");
    hdtn::HdtnRegsvr storeReg;
    hdtn::HdtnRegsvr telemReg;
    const std::string connect_regServerPath(
        std::string("tcp://") +
        m_hdtnConfig.m_zmqRegistrationServerAddress +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqRegistrationServerPortPath));
    storeReg.Init(connect_regServerPath, "storage", m_hdtnConfig.m_zmqConnectingStorageToBoundEgressPortPath, "push");
    telemReg.Init(connect_regServerPath, "c2/telem", 10460, "rep"); //TODO FIX
    storeReg.Reg();
    telemReg.Reg();
    std::cout << "[storage] Registration completed." << std::endl;
    hdtn::Logger::getInstance()->logNotification("storage", "Registration Completed");

    

    hdtn::HdtnEntries_ptr entries = storeReg.Query("ingress");
    while (!entries || entries->m_hdtnEntryList.empty()) {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        std::cout << "[storage] Waiting for available ingress system ..." << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "[storage] Waiting for available ingress system ...");
        entries = storeReg.Query("ingress");
    }
    const hdtn::HdtnEntryList_t & entryList = entries->m_hdtnEntryList;

    std::string remote = entryList.front().protocol + "://" + entryList.front().address + ":" + std::to_string(entryList.front().port);
    std::cout << "[storage] Found available ingress: " << remote << " - connecting ..." << std::endl;
    hdtn::Logger::getInstance()->logNotification("storage", "[storage] Found available ingress: " + remote + " - connecting ...");

    m_zmqContextPtr = boost::make_unique<zmq::context_t>();
    //telemetry not implemnted yet
    m_telemetrySockPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::rep);
    m_telemetrySockPtr->bind(HDTN_STORAGE_TELEM_PATH);

    if (hdtnOneProcessZmqInprocContextPtr) {
        m_zmqPullSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        try {
            //m_ingressSockPtr->connect(remote);
            m_zmqPullSock_boundIngressToConnectingStoragePtr->connect(std::string("inproc://bound_ingress_to_connecting_storage"));
        }
        catch (const zmq::error_t & ex) {
            std::cerr << "error: cannot connect ingress socket: " << ex.what() << std::endl;
            hdtn::Logger::getInstance()->logError("storage", "Error: cannot connect ingress socket: " + std::string(ex.what()));
            return false;
        }

       
    }
    else {

        m_zmqPullSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pull);
        const std::string connect_boundIngressToConnectingStoragePath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqIngressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundIngressToConnectingStoragePortPath));
        try {
            //m_ingressSockPtr->connect(remote);
            m_zmqPullSock_boundIngressToConnectingStoragePtr->connect(connect_boundIngressToConnectingStoragePath);
        }
        catch (const zmq::error_t & ex) {
            std::cerr << "error: cannot connect ingress socket: " << ex.what() << std::endl;
            hdtn::Logger::getInstance()->logError("storage", "Error: cannot connect ingress socket: " + std::string(ex.what()));
            return false;
        }
    }

    m_zmqSubSock_boundReleaseToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::sub);
    const std::string connect_boundSchedulerPubSubPath(
        std::string("tcp://") +
        m_hdtnConfig.m_zmqSchedulerAddress +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));
    try {
        m_zmqSubSock_boundReleaseToConnectingStoragePtr->connect(connect_boundSchedulerPubSubPath);// config.releaseWorker);
        m_zmqSubSock_boundReleaseToConnectingStoragePtr->set(zmq::sockopt::subscribe, "");
        std::cout << "release sock connected to " << connect_boundSchedulerPubSubPath << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "Release sock connected to " + connect_boundSchedulerPubSubPath);
    }
    catch (const zmq::error_t & ex) {
        std::cerr << "error: cannot connect release socket: " << ex.what() << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "Error: cannot connect release socket: " + std::string(ex.what()));
        return false;
    }
    
    std::cout << "[storage] Spinning up worker thread ..." << std::endl;
    hdtn::Logger::getInstance()->logNotification("storage", "[storage] Spinning up worker thread ...");
    m_workerSockPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pair);
    m_workerSockPtr->bind(HDTN_STORAGE_WORKER_PATH);
    worker.Init(m_zmqContextPtr.get(), m_hdtnConfig, hdtnOneProcessZmqInprocContextPtr);
    worker.launch();
    zmq::message_t tmsg;
    if (!m_workerSockPtr->recv(tmsg, zmq::recv_flags::none)) {
        std::cerr << "[storage] Worker startup failed (no receive) - aborting ..." << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "[storage] Worker startup failed (no receive) - aborting ...");
        return false;
    }
    CommonHdr *notify = (CommonHdr *)tmsg.data();
    if (notify->type != HDTN_MSGTYPE_IOK) {
        std::cout << "[storage] Worker startup failed - aborting ..." << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "[storage] Worker startup failed - aborting ...");
        return false;
    }
    std::cout << "[storage] Verified worker startup." << std::endl;
    hdtn::Logger::getInstance()->logNotification("storage", "Verified worker startup");

    std::cout << "[storage] Done." << std::endl;
    return true;
}


void hdtn::storage::update() {
    zmq::pollitem_t items[] = {
        {m_zmqPullSock_boundIngressToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundReleaseToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_telemetrySockPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    if (zmq::poll(&items[0], 3, 250) > 0) {
        if (items[0].revents & ZMQ_POLLIN) {
            dispatch();
        }
        if (items[1].revents & ZMQ_POLLIN) {
            std::cout << "release" << std::endl;
            hdtn::Logger::getInstance()->logNotification("storage", "release");
            scheduleRelease();
        }
        if (items[2].revents & ZMQ_POLLIN) {
            c2telem();  //not implemented yet
        }
    }
}

void hdtn::storage::c2telem() {
    zmq::message_t message;
    if (!m_telemetrySockPtr->recv(message, zmq::recv_flags::none)) {
        std::cerr << "[c2telem] message not received" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[c2telem] message not received");
        return;
    }
    if (message.size() < sizeof(CommonHdr)) {
        std::cerr << "[c2telem] message too short: " << message.size() << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[c2telem] message too short: " + std::to_string(message.size()));
        return;
    }
    CommonHdr *common = (CommonHdr *)message.data();
    switch (common->type) {
        case HDTN_MSGTYPE_CSCHED_REQ:
            break;
        case HDTN_MSGTYPE_CTELEM_REQ:
            break;
    }
}
void hdtn::storage::scheduleRelease() {
    //  storageStats.in_bytes += hdr.size();
    //++storageStats.in_msg;
    zmq::message_t message;
    if (!m_zmqSubSock_boundReleaseToConnectingStoragePtr->recv(message, zmq::recv_flags::none)) {
        std::cerr << "[schedule release] message not received" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[schedule release] message not received");
        return;
    }
    if (message.size() < sizeof(CommonHdr)) {
        std::cerr << "[schedule release] message too short: " << message.size() << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[schedule release] message too short: " + std::to_string(message.size()));
        return;
    }
    std::cout << "message received\n";
    hdtn::Logger::getInstance()->logNotification("storage", "Message received");
    CommonHdr *common = (CommonHdr *)message.data();
    switch (common->type) {
        case HDTN_MSGTYPE_IRELSTART:
            std::cout << "release data\n";
            hdtn::Logger::getInstance()->logNotification("storage", "Release data");
            m_workerSockPtr->send(message, zmq::send_flags::none); //VERIFY this works over const_buffer message.data(), message.size(), 0); (tested and apparently it does)
            storageStats.worker = worker.stats();
            break;
        case HDTN_MSGTYPE_IRELSTOP:
            std::cout << "stop releasing data\n";
            hdtn::Logger::getInstance()->logNotification("storage", "Stop releasing data");
            break;
    }
}
void hdtn::storage::dispatch() {
    zmq::message_t hdr;
    zmq::message_t message;
    if (!m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(hdr, zmq::recv_flags::none)) {
        std::cerr << "[dispatch] message hdr not received" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[dispatch] message hdr not received");
        return;
    }
    storageStats.inBytes += hdr.size();
    ++storageStats.inMsg;

    if (hdr.size() < sizeof(CommonHdr)) {
        std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[dispatch] message too short: " + std::to_string(hdr.size()));
        return;
    }
    CommonHdr *common = (CommonHdr *)hdr.data();
    BlockHdr *block = (BlockHdr *)common;
    switch (common->type) {
        case HDTN_MSGTYPE_STORE:
            if (!m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(message, zmq::recv_flags::none)) {
                std::cerr << "[dispatch] message not received" << std::endl;
                hdtn::Logger::getInstance()->logError("storage", "[dispatch] message not received");
                return;
            }
            //std::cout << "rxptr: " << (std::uintptr_t)(message.data()) << std::endl;
            /*if(message.size() < 7000){
                std::cout<<"ingress sent less than 7000, type "<< common->type << "size " <<  message.size()<<"\n";
            }*/
            m_workerSockPtr->send(hdr, zmq::send_flags::none);//m_workerSockPtr->send(hdr.data(), hdr.size(), ZMQ_MORE);
            storageStats.inBytes += message.size();
            m_workerSockPtr->send(message, zmq::send_flags::none);//m_workerSockPtr->send(message.data(), message.size(), 0);
            break;
    }
}

std::size_t hdtn::storage::GetCurrentNumberOfBundlesDeletedFromStorage() {
    return worker.m_totalBundlesErasedFromStorage;
}

