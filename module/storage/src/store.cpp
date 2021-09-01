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
    m_inprocBundleDataSockPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pair);
    m_inprocBundleDataSockPtr->bind(HDTN_STORAGE_BUNDLE_DATA_INPROC_PATH);
    m_inprocReleaseMessagesSockPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pair);
    m_inprocReleaseMessagesSockPtr->bind(HDTN_STORAGE_RELEASE_MESSAGES_INPROC_PATH);
    m_pollItems[0] = { m_zmqPullSock_boundIngressToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0 };
    m_pollItems[1] = { m_zmqSubSock_boundReleaseToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0 };
    m_pollItems[2] = { m_telemetrySockPtr->handle(), 0, ZMQ_POLLIN, 0 };
    worker.Init(m_zmqContextPtr.get(), m_hdtnConfig, hdtnOneProcessZmqInprocContextPtr);
    worker.launch();
    zmq::message_t tmsg;
    if (!m_inprocBundleDataSockPtr->recv(tmsg, zmq::recv_flags::none)) {
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
    if (zmq::poll(&m_pollItems[0], 3, 250) > 0) {
        if (m_pollItems[0].revents & ZMQ_POLLIN) {
            dispatch();
        }
        if (m_pollItems[1].revents & ZMQ_POLLIN) {
            std::cout << "release" << std::endl;
            hdtn::Logger::getInstance()->logNotification("storage", "release");
            scheduleRelease();
        }
        if (m_pollItems[2].revents & ZMQ_POLLIN) {
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
static void CustomCleanupVec64(void *data, void *hint) {
    //std::cout << "free " << static_cast<std::vector<uint64_t>*>(hint)->size() << std::endl;
    delete static_cast<std::vector<uint64_t>*>(hint);
}
void hdtn::storage::scheduleRelease() {
    //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
    static constexpr std::size_t minBufSizeBytes = sizeof(uint64_t) + ((sizeof(IreleaseStartHdr) > sizeof(IreleaseStopHdr)) ? sizeof(IreleaseStartHdr) : sizeof(IreleaseStopHdr));
    std::vector<uint64_t> * rxBufPtrToStdVec64 = new std::vector<uint64_t>(minBufSizeBytes / sizeof(uint64_t));
    std::vector<uint64_t> & vec64Ref = *rxBufPtrToStdVec64;
    uint64_t * rxBufRawPtrAlign64 = &vec64Ref[0];
    zmq::message_t messageWithDataStolen(rxBufRawPtrAlign64, minBufSizeBytes, CustomCleanupVec64, rxBufPtrToStdVec64);
    const zmq::recv_buffer_result_t res = m_zmqSubSock_boundReleaseToConnectingStoragePtr->recv(zmq::mutable_buffer(rxBufRawPtrAlign64, minBufSizeBytes), zmq::recv_flags::none);
    if (!res) {
        std::cerr << "[schedule release] message not received" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[schedule release] message not received");
        return;
    }
    else if (res->size < sizeof(hdtn::CommonHdr)) {
        std::cerr << "[schedule release] res->size < sizeof(hdtn::CommonHdr)" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[schedule release] res->size < sizeof(hdtn::CommonHdr)");
        return;
    }
    
    std::cout << "message received\n";
    hdtn::Logger::getInstance()->logNotification("storage", "Message received");
    CommonHdr *common = (CommonHdr *)rxBufRawPtrAlign64;
    switch (common->type) {
        case HDTN_MSGTYPE_IRELSTART:
            if (res->size != sizeof(hdtn::IreleaseStartHdr)) {
                std::cerr << "[schedule release] res->size != sizeof(hdtn::IreleaseStartHdr" << std::endl;
                hdtn::Logger::getInstance()->logError("storage", "[schedule release] res->size != sizeof(hdtn::IreleaseStartHdr");
                return;
            }
            std::cout << "release data\n";
            hdtn::Logger::getInstance()->logNotification("storage", "Release data");
            m_inprocReleaseMessagesSockPtr->send(messageWithDataStolen, zmq::send_flags::none);
            storageStats.worker = worker.stats();
            break;
        case HDTN_MSGTYPE_IRELSTOP:
            if (res->size != sizeof(hdtn::IreleaseStopHdr)) {
                std::cerr << "[schedule release] res->size != sizeof(hdtn::IreleaseStopHdr" << std::endl;
                hdtn::Logger::getInstance()->logError("storage", "[schedule release] res->size != sizeof(hdtn::IreleaseStopHdr");
                return;
            }
            std::cout << "stop releasing data\n";
            hdtn::Logger::getInstance()->logNotification("storage", "Stop releasing data");
            m_inprocReleaseMessagesSockPtr->send(messageWithDataStolen, zmq::send_flags::none);
            storageStats.worker = worker.stats();
            break;
    }
}
static void CustomCleanupToStorageHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToStorageHdr*>(hint);
}
void hdtn::storage::dispatch() {
    hdtn::ToStorageHdr * toStorageHeader = new hdtn::ToStorageHdr();
    zmq::message_t zmqMessageToStorageHdrWithDataStolen(toStorageHeader, sizeof(hdtn::ToStorageHdr), CustomCleanupToStorageHdr, toStorageHeader);
    const zmq::recv_buffer_result_t res = m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(zmq::mutable_buffer(toStorageHeader, sizeof(hdtn::ToStorageHdr)), zmq::recv_flags::none);
    if (!res) {
        std::cerr << "[dispatch] message hdr not received" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[dispatch] message hdr not received");
        return;
    }
    else if ((res->truncated()) || (res->size != sizeof(hdtn::ToStorageHdr))) {
        std::cerr << "[dispatch] message hdr not sizeof(hdtn::ToStorageHdr)" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[dispatch] message hdr not sizeof(hdtn::ToStorageHdr)");
        return;
    }
    else if (toStorageHeader->base.type != HDTN_MSGTYPE_STORE) {
        std::cerr << "[dispatch] message type not HDTN_MSGTYPE_STORE" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[dispatch] message type not HDTN_MSGTYPE_STORE");
        return;
    }

    storageStats.inBytes += sizeof(hdtn::ToStorageHdr);
    ++storageStats.inMsg;

    zmq::message_t message;
    if (!m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(message, zmq::recv_flags::none)) {
        std::cerr << "[dispatch] message not received" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "[dispatch] message not received");
        return;
    }
    m_inprocBundleDataSockPtr->send(std::move(zmqMessageToStorageHdrWithDataStolen), zmq::send_flags::sndmore);
    storageStats.inBytes += message.size();
    m_inprocBundleDataSockPtr->send(std::move(message), zmq::send_flags::none);
            
}

std::size_t hdtn::storage::GetCurrentNumberOfBundlesDeletedFromStorage() {
    return worker.m_totalBundlesErasedFromStorage;
}

