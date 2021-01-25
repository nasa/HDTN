#include "store.hpp"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#include "cache.hpp"
#include "message.hpp"

#define HDTN_STORAGE_TYPE "storage"
#define HDTN_STORAGE_RECV_MODE "push"

bool hdtn::storage::init(storageConfig config) {
    if (config.local.find(":") == std::string::npos) {
        throw error_t();
    }
    std::string path = config.local.substr(config.local.find(":") + 1);
    if (path.find(":") == std::string::npos) {
        throw error_t();
    }
    int port = atoi(path.substr(path.find(":") + 1).c_str());
    if (port < 1024) {
        throw error_t();
    }
    std::string telem_path = config.telem.substr(config.telem.find(":") + 1);
    if (telem_path.find(":") == std::string::npos) {
        throw error_t();
    }
    int telem_port = atoi(telem_path.substr(telem_path.find(":") + 1).c_str());

    std::cout << "[storage] Executing registration ..." << std::endl;
    storeReg.init(config.regsvr, "storage", port, "push");
    telemReg.init(config.regsvr, "c2/telem", telem_port, "rep");
    storeReg.reg();
    telemReg.reg();
    std::cout << "[storage] Registration completed." << std::endl;

    zmqContext = new zmq::context_t;
    //telemetry not implemnted yet
    telemetrySock = new zmq::socket_t(*zmqContext, zmq::socket_type::rep);
    telemetrySock->bind(config.telem);

    hdtn_entries entries = storeReg.query("ingress");
    while (entries.size() == 0) {
        sleep(1);
        std::cout << "[storage] Waiting for available ingress system ..." << std::endl;
        entries = storeReg.query("ingress");
    }

    std::string remote = entries.front().protocol + "://" + entries.front().address + ":" + std::to_string(entries.front().port);
    std::cout << "[storage] Found available ingress: " << remote << " - connecting ..." << std::endl;
    int res = static_cast<int>(ingress(remote));
    if (!res) {
        return res;
    }

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
    releaseSock = new zmq::socket_t(*zmqContext, zmq::socket_type::sub);
    bool success = true;
    try {
        releaseSock->connect(config.releaseWorker);
        releaseSock->setsockopt(ZMQ_SUBSCRIBE, "", 0);
    } catch (error_t ex) {
        success = false;
    }
    std::cout << "[storage] Spinning up worker thread ..." << std::endl;
    workerSock = new zmq::socket_t(*zmqContext, zmq::socket_type::pair);
    workerSock->bind(config.worker);
    worker.init(zmqContext, config);
    worker.launch();
    zmq::message_t tmsg;
    workerSock->recv(&tmsg);
    common_hdr *notify = (common_hdr *)tmsg.data();
    if (notify->type != HDTN_MSGTYPE_IOK) {
        std::cout << "[storage] Worker startup failed - aborting ..." << std::endl;
        return false;
    }
    std::cout << "[storage] Verified worker startup." << std::endl;

    std::cout << "[storage] Done." << std::endl;
    return true;
}

bool hdtn::storage::ingress(std::string remote) {
    ingressSock = new zmq::socket_t(*zmqContext, zmq::socket_type::pull);
    bool success = true;
    try {
        ingressSock->connect(remote);
    } catch (error_t ex) {
        success = false;
    }

    return success;
}

void hdtn::storage::update() {
    zmq::pollitem_t items[] = {
        {ingressSock->handle(),
         0,
         ZMQ_POLLIN,
         0},
        {releaseSock->handle(),
         0,
         ZMQ_POLLIN,
         0},
        {telemetrySock->handle(),
         0,
         ZMQ_POLLIN,
         0},
    };
    zmq::poll(&items[0], 2, 0);
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

void hdtn::storage::c2telem() {
    zmq::message_t message;
    telemetrySock->recv(&message);
    if (message.size() < sizeof(hdtn::common_hdr)) {
        std::cerr << "[c2telem] message too short: " << message.size() << std::endl;
        return;
    }
    hdtn::common_hdr *common = (hdtn::common_hdr *)message.data();
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
    releaseSock->recv(&message);
    if (message.size() < sizeof(hdtn::common_hdr)) {
        std::cerr << "[dispatch] message too short: " << message.size() << std::endl;
        return;
    }
    std::cout << "message received\n";
    hdtn::common_hdr *common = (hdtn::common_hdr *)message.data();
    switch (common->type) {
        case HDTN_MSGTYPE_IRELSTART:
            std::cout << "release data\n";
            workerSock->send(message.data(), message.size(), 0);
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
    ingressSock->recv(&hdr);
    storageStats.in_bytes += hdr.size();
    ++storageStats.in_msg;

    if (hdr.size() < sizeof(hdtn::common_hdr)) {
        std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
        return;
    }
    hdtn::common_hdr *common = (hdtn::common_hdr *)hdr.data();
    hdtn::block_hdr *block = (hdtn::block_hdr *)common;
    switch (common->type) {
        case HDTN_MSGTYPE_STORE:
            ingressSock->recv(&message);
            /*if(message.size() < 7000){
                std::cout<<"ingress sent less than 7000, type "<< common->type << "size " <<  message.size()<<"\n";
            }*/
            workerSock->send(hdr.data(), hdr.size(), ZMQ_MORE);
            storageStats.in_bytes += message.size();
            workerSock->send(message.data(), message.size(), 0);
            break;
    }
}
