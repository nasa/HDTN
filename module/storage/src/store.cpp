#include <unistd.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "message.hpp"
#include "store.hpp"
#include "cache.hpp"

#define HDTN_STORAGE_TYPE         "storage"
#define HDTN_STORAGE_RECV_MODE    "push"

bool hdtn::storage::init(storage_config config) {
    if(config.local.find(":") == std::string::npos) {
        throw error_t();
    }
    std::string path = config.local.substr(config.local.find(":") + 1);
    if(path.find(":") == std::string::npos) {
        throw error_t();
    }
    int port = atoi(path.substr(path.find(":") + 1).c_str());
    if(port < 1024) {
        throw error_t();
    }
    std::string telem_path = config.telem.substr(config.telem.find(":") + 1);
    if(telem_path.find(":") == std::string::npos) {
        throw error_t();
    }
    int telem_port = atoi(telem_path.substr(telem_path.find(":") + 1).c_str());

    std::cout << "[storage] Executing registration ..." << std::endl;
    _store_reg.init(config.regsvr, "storage", port, "push");
    _telem_reg.init(config.regsvr, "c2/telem", telem_port, "rep");
    _store_reg.reg();
    _telem_reg.reg();
    std::cout << "[storage] Registration completed." << std::endl;

    _ctx = new zmq::context_t;

    _telemetry_sock = new zmq::socket_t(*_ctx, zmq::socket_type::rep);
    _telemetry_sock->bind(config.telem);

    _egress_sock = new zmq::socket_t(*_ctx, zmq::socket_type::push);
    _egress_sock->bind(config.local);

    hdtn_entries entries = _store_reg.query("ingress");
    while(entries.size() == 0) {
        sleep(1);
        std::cout << "[storage] Waiting for available ingress system ..." << std::endl;
        entries = _store_reg.query("ingress");
    }

    std::string remote = entries.front().protocol + "://" + entries.front().address + ":" + std::to_string(entries.front().port);
    std::cout << "[storage] Found available ingress: " << remote << " - connecting ..." << std::endl;
    int res = static_cast<int>(ingress(remote));
    if(!res) {
        return res;
    }

    std::cout << "[storage] Preparing flow cache ... " << std::endl;
    struct stat cache_info;
    res = stat(config.store_path.c_str(), &cache_info);
    if(res) {
        switch(errno) {
            case ENOENT:
            break;
            case ENOTDIR:
            std::cerr << "Failed to open cache - at least one element in specified path is not a directory." << std::endl;
            return false;
            default:
            perror("Failed to open cache - ");
            return false;
        }

        if(errno == ENOENT) {
            std::cout << "[storage] Attempting to create cache: " << config.store_path << std::endl;
            mkdir(config.store_path.c_str(), S_IRWXU);
            res = stat(config.store_path.c_str(), &cache_info);
            if(res) {
                perror("Failed to create file cache - ");
                return false;
            }
        }
    }

    std::cout << "[storage] Spinning up worker thread ..." << std::endl;
    _worker_sock = new zmq::socket_t(*_ctx, zmq::socket_type::pair);
    _worker_sock->bind(config.worker);
    _worker.init(_ctx, config);
    _worker.launch();
    zmq::message_t tmsg;
    _worker_sock->recv(&tmsg);
    common_hdr* notify = (common_hdr*)tmsg.data();
    if(notify->type != HDTN_MSGTYPE_IOK) {
        std::cout << "[storage] Worker startup failed - aborting ..." << std::endl;
        return false;
    }
    std::cout << "[storage] Verified worker startup." << std::endl;
    
    std::cout << "[storage] Done." << std::endl;
    return true;
}

bool hdtn::storage::ingress(std::string remote) {
    _ingress_sock = new zmq::socket_t(*_ctx, zmq::socket_type::pull);
    bool success = true;
    try {
        _ingress_sock->connect(remote);
    }
    catch(error_t ex) {
        success = false;
    }

    return success;
}

void hdtn::storage::update() {
    zmq::pollitem_t items[] = {
        {
            _ingress_sock->handle(),
            0,
            ZMQ_POLLIN,
            0
        },
        {
            _telemetry_sock->handle(),
            0,
            ZMQ_POLLIN,
            0
        }
    };
    zmq::poll(&items[0], 2, 0);
    if(items[0].revents & ZMQ_POLLIN) {
        dispatch();
    }
    if(items[1].revents & ZMQ_POLLIN) {
        c2telem();
    }
}

void hdtn::storage::c2telem() {
    zmq::message_t message;
    _telemetry_sock->recv(&message);
    if(message.size() < sizeof(hdtn::common_hdr)) {
        std::cerr << "[c2telem] message too short: " << message.size() << std::endl;
        return;
    }
    hdtn::common_hdr* common = (hdtn::common_hdr*)message.data();
    switch(common->type) {
        case HDTN_MSGTYPE_CSCHED_REQ:
        break;
        case HDTN_MSGTYPE_CTELEM_REQ:
        break;
    }
}

void hdtn::storage::dispatch() {
    zmq::message_t hdr;
    zmq::message_t message;
    _ingress_sock->recv(&hdr);
    _stats.in_bytes += hdr.size();
    ++_stats.in_msg;

    if(hdr.size() < sizeof(hdtn::common_hdr)) {
        std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
        return;
    }
    hdtn::common_hdr* common = (hdtn::common_hdr*)hdr.data();
    hdtn::block_hdr* block = (hdtn::block_hdr *)common;
    switch(common->type) {
        case HDTN_MSGTYPE_STORE:
        _ingress_sock->recv(&message);
        _worker_sock->send(hdr.data(), hdr.size(), ZMQ_MORE);
        _stats.in_bytes += message.size();
        _worker_sock->send(message.data(), message.size(), 0);
        break;
    }
}

void hdtn::storage::release(uint32_t flow, uint64_t rate, uint64_t duration) {

}
