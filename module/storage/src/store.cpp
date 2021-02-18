#include "store.hpp"
#include <boost/make_shared.hpp>
#include <iostream>
#include "cache.hpp"
#include "message.hpp"

#define HDTN_STORAGE_TYPE "storage"
#define HDTN_STORAGE_RECV_MODE "push"

bool hdtn::storage::init(storageConfig config) {
    if (config.local.find(":") == std::string::npos) {
        return false;// throw error_t();
    }
    std::string path = config.local.substr(config.local.find(":") + 1);
    if (path.find(":") == std::string::npos) {
        return false;// throw error_t();
    }
    int port = atoi(path.substr(path.find(":") + 1).c_str());
    if (port < 1024) {
        return false;//throw error_t();
    }
    std::string telem_path = config.telem.substr(config.telem.find(":") + 1);
    if (telem_path.find(":") == std::string::npos) {
        return false;//throw error_t();
    }
    int telem_port = atoi(telem_path.substr(telem_path.find(":") + 1).c_str());

    std::cout << "[storage] Executing registration ..." << std::endl;
    hdtn::HdtnRegsvr storeReg;
    hdtn::HdtnRegsvr telemReg;
    storeReg.Init(config.regsvr, "storage", port, "push");
    telemReg.Init(config.regsvr, "c2/telem", telem_port, "rep");
    storeReg.Reg();
    telemReg.Reg();
    std::cout << "[storage] Registration completed." << std::endl;


    m_zmqContextPtr = boost::make_shared<zmq::context_t>();
    //telemetry not implemnted yet
    m_telemetrySockPtr = boost::make_shared<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::rep);
    m_telemetrySockPtr->bind(config.telem);

    hdtn::HdtnEntries_ptr entries = storeReg.Query("ingress");
    while (!entries) {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        std::cout << "[storage] Waiting for available ingress system ..." << std::endl;
        entries = storeReg.Query("ingress");
    }
    const hdtn::HdtnEntryList_t & entryList = entries->m_hdtnEntryList;


    std::string remote = entryList.front().protocol + "://" + entryList.front().address + ":" + std::to_string(entryList.front().port);
    std::cout << "[storage] Found available ingress: " << remote << " - connecting ..." << std::endl;

    m_ingressSockPtr = boost::make_shared<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pull);
    try {
        m_ingressSockPtr->connect(remote);
    }
    catch (const zmq::error_t & ex) {
        std::cerr << "error: cannot connect ingress socket: " << ex.what() << std::endl;
        return false;
    }

#ifndef USE_BRIAN_STORAGE
    std::cout << "[storage] Preparing flow cache ... " << std::endl;
    struct stat cache_info;
    res = stat(config.storePath.c_str(), &cache_info);
    if (res) {
        switch (errno) {
            case ENOENT:
                break;
            case ENOTDIR:
                std::cerr << "Failed to open cache - at least one element in specified path is not a directory." << std::endl;
                return false;
            default:
                perror("Failed to open cache - ");
                return false;
        }

        if (errno == ENOENT) {
            std::cout << "[storage] Attempting to create cache: " << config.storePath << std::endl;
            mkdir(config.storePath.c_str(), S_IRWXU);
            res = stat(config.storePath.c_str(), &cache_info);
            if (res) {
                perror("Failed to create file cache - ");
                return false;
            }
        }
    }
#endif //not using USE_BRIAN_STORAGE
    m_releaseSockPtr = boost::make_shared<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::sub);
    try {
        m_releaseSockPtr->connect(config.releaseWorker);
        m_releaseSockPtr->setsockopt(ZMQ_SUBSCRIBE, "", 0);
    } catch (const zmq::error_t & ex) {
        std::cerr << "error: cannot connect release socket: " << ex.what() << std::endl;
        return false;
    }

    std::cout << "[storage] Spinning up worker thread ..." << std::endl;
    m_workerSockPtr = boost::make_shared<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pair);
    m_workerSockPtr->bind(config.worker);
    worker.init(m_zmqContextPtr.get(), config);
    worker.launch();
    zmq::message_t tmsg;
    m_workerSockPtr->recv(&tmsg);
    CommonHdr *notify = (CommonHdr *)tmsg.data();
    if (notify->type != HDTN_MSGTYPE_IOK) {
        std::cout << "[storage] Worker startup failed - aborting ..." << std::endl;
        return false;
    }
    std::cout << "[storage] Verified worker startup." << std::endl;

    std::cout << "[storage] Done." << std::endl;
    return true;
}


void hdtn::storage::update() {
    zmq::pollitem_t items[] = {
        {m_ingressSockPtr->handle(),
         0,
         ZMQ_POLLIN,
         0},
        {m_releaseSockPtr->handle(),
         0,
         ZMQ_POLLIN,
         0},
        {m_telemetrySockPtr->handle(),
         0,
         ZMQ_POLLIN,
         0},
    };
    if (zmq::poll(&items[0], 3, 250) > 0) {
        if (items[0].revents & ZMQ_POLLIN) {
            dispatch();
        }
        if (items[1].revents & ZMQ_POLLIN) {
            scheduleRelease();
        }
        if (items[2].revents & ZMQ_POLLIN) {
            c2telem();  //not implemented yet
        }
    }
}

void hdtn::storage::c2telem() {
    zmq::message_t message;
    m_telemetrySockPtr->recv(&message);
    if (message.size() < sizeof(CommonHdr)) {
        std::cerr << "[c2telem] message too short: " << message.size() << std::endl;
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
    m_releaseSockPtr->recv(&message);
    if (message.size() < sizeof(CommonHdr)) {
        std::cerr << "[dispatch] message too short: " << message.size() << std::endl;
        return;
    }
    std::cout << "message received\n";
    CommonHdr *common = (CommonHdr *)message.data();
    switch (common->type) {
        case HDTN_MSGTYPE_IRELSTART:
            std::cout << "release data\n";
            m_workerSockPtr->send(message.data(), message.size(), 0);
            storageStats.worker = worker.stats();
            break;
        case HDTN_MSGTYPE_IRELSTOP:
            std::cout << "stop releasing data\n";
            break;
    }
}
void hdtn::storage::dispatch() {
    zmq::message_t hdr;
    zmq::message_t message;
    m_ingressSockPtr->recv(&hdr);
    storageStats.inBytes += hdr.size();
    ++storageStats.inMsg;

    if (hdr.size() < sizeof(CommonHdr)) {
        std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
        return;
    }
    CommonHdr *common = (CommonHdr *)hdr.data();
    BlockHdr *block = (BlockHdr *)common;
    switch (common->type) {
        case HDTN_MSGTYPE_STORE:
            m_ingressSockPtr->recv(&message);
            /*if(message.size() < 7000){
                std::cout<<"ingress sent less than 7000, type "<< common->type << "size " <<  message.size()<<"\n";
            }*/
            m_workerSockPtr->send(hdr.data(), hdr.size(), ZMQ_MORE);
            storageStats.inBytes += message.size();
            m_workerSockPtr->send(message.data(), message.size(), 0);
            break;
    }
}
