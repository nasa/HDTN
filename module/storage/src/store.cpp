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

bool hdtn::Storage::Init(StorageConfig config) {
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
    std::string telemPath = config.telem.substr(config.telem.find(":") + 1);
    if (telemPath.find(":") == std::string::npos) {
        throw error_t();
    }
    int telem_port = atoi(telemPath.substr(telemPath.find(":") + 1).c_str());

    std::cout << "[storage] Executing registration ..." << std::endl;
    m_storeReg.Init(config.regsvr, "storage", port, "push");
    m_telemReg.Init(config.regsvr, "c2/telem", telem_port, "rep");
    m_storeReg.Reg();
    m_telemReg.Reg();
    std::cout << "[storage] Registration completed." << std::endl;

    m_ctx = new zmq::context_t;

    m_telemetrySock = new zmq::socket_t(*m_ctx, zmq::socket_type::rep);
    m_telemetrySock->bind(config.telem);

    m_egressSock = new zmq::socket_t(*m_ctx, zmq::socket_type::push);
    m_egressSock->bind(config.local);

    hdtn::HdtnEntries_ptr entries = m_storeReg.Query("ingress");
    while (!entries) {
        sleep(1);
        std::cout << "[storage] Waiting for available ingress system ..." << std::endl;
        entries = m_storeReg.Query("ingress");
    }
    const hdtn::HdtnEntryList_t & entryList = entries->m_hdtnEntryList;

    std::string remote =
        entryList.front().protocol + "://" + entryList.front().address + ":" + std::to_string(entryList.front().port);
    std::cout << "[storage] Found available ingress: " << remote << " - connecting ..." << std::endl;
    int res = static_cast<int>(Ingress(remote));
    if (!res) {
        return res;
    }

    std::cout << "[storage] Preparing flow cache ... " << std::endl;
    struct stat cacheInfo;
    res = stat(config.storePath.c_str(), &cacheInfo);
    if (res) {
        switch (errno) {
            case ENOENT:
                break;
            case ENOTDIR:
                std::cerr << "Failed to open cache - at least one element in specified "
                             "path is not a directory."
                          << std::endl;
                return false;
            default:
                perror("Failed to open cache - ");
                return false;
        }

        if (errno == ENOENT) {
            std::cout << "[storage] Attempting to create cache: " << config.storePath << std::endl;
            mkdir(config.storePath.c_str(), S_IRWXU);
            res = stat(config.storePath.c_str(), &cacheInfo);
            if (res) {
                perror("Failed to create file cache - ");
                return false;
            }
        }
    }

    std::cout << "[storage] Spinning up worker thread ..." << std::endl;
    m_workerSock = new zmq::socket_t(*m_ctx, zmq::socket_type::pair);
    m_workerSock->bind(config.worker);
    m_worker.Init(m_ctx, config);
    m_worker.Launch();
    zmq::message_t tmsg;
    m_workerSock->recv(&tmsg);
    CommonHdr *notify = (CommonHdr *)tmsg.data();
    if (notify->type != HDTN_MSGTYPE_IOK) {
        std::cout << "[storage] Worker startup failed - aborting ..." << std::endl;
        return false;
    }
    std::cout << "[storage] Verified worker startup." << std::endl;

    std::cout << "[storage] Done." << std::endl;
    return true;
}

bool hdtn::Storage::Ingress(std::string remote) {
    m_ingressSock = new zmq::socket_t(*m_ctx, zmq::socket_type::pull);
    bool success = true;
    try {
        m_ingressSock->connect(remote);
    } catch (error_t ex) {
        success = false;
    }

    return success;
}

void hdtn::Storage::Update() {
    zmq::pollitem_t items[] = {{m_ingressSock->handle(), 0, ZMQ_POLLIN, 0},
                               {m_telemetrySock->handle(), 0, ZMQ_POLLIN, 0}};
    zmq::poll(&items[0], 2, 0);
    if (items[0].revents & ZMQ_POLLIN) {
        Dispatch();
    }
    if (items[1].revents & ZMQ_POLLIN) {
        C2telem();
    }
}

void hdtn::Storage::C2telem() {
    zmq::message_t message;
    m_telemetrySock->recv(&message);
    if (message.size() < sizeof(hdtn::CommonHdr)) {
        std::cerr << "[c2telem] message too short: " << message.size() << std::endl;
        return;
    }
    hdtn::CommonHdr *common = (hdtn::CommonHdr *)message.data();
    switch (common->type) {
        case HDTN_MSGTYPE_CSCHED_REQ:
            break;
        case HDTN_MSGTYPE_CTELEM_REQ:
            break;
    }
}

void hdtn::Storage::Dispatch() {
    zmq::message_t hdr;
    zmq::message_t message;
    m_ingressSock->recv(&hdr);
    m_stats.inBytes += hdr.size();
    ++m_stats.inMsg;

    if (hdr.size() < sizeof(hdtn::CommonHdr)) {
        std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
        return;
    }
    hdtn::CommonHdr *common = (hdtn::CommonHdr *)hdr.data();
    hdtn::BlockHdr *block = (hdtn::BlockHdr *)common;
    switch (common->type) {
        case HDTN_MSGTYPE_STORE:
            m_ingressSock->recv(&message);
            m_workerSock->send(hdr.data(), hdr.size(), ZMQ_MORE);
            m_stats.inBytes += message.size();
            m_workerSock->send(message.data(), message.size(), 0);
            break;
    }
}

void hdtn::Storage::Release(uint32_t flow, uint64_t rate, uint64_t duration) {}
