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
  std::string telem_path = config.telem.substr(config.telem.find(":") + 1);
  if (telem_path.find(":") == std::string::npos) {
    throw error_t();
  }
  int telem_port = atoi(telem_path.substr(telem_path.find(":") + 1).c_str());

  std::cout << "[storage] Executing registration ..." << std::endl;
  store_reg_.Init(config.regsvr, "storage", port, "push");
  telem_reg_.Init(config.regsvr, "c2/telem", telem_port, "rep");
  store_reg_.Reg();
  telem_reg_.Reg();
  std::cout << "[storage] Registration completed." << std::endl;

  ctx_ = new zmq::context_t;

  telemetry_sock_ = new zmq::socket_t(*ctx_, zmq::socket_type::rep);
  telemetry_sock_->bind(config.telem);

  egress_sock_ = new zmq::socket_t(*ctx_, zmq::socket_type::push);
  egress_sock_->bind(config.local);

  hdtn_entries entries = store_reg_.Query("ingress");
  while (entries.size() == 0) {
    sleep(1);
    std::cout << "[storage] Waiting for available ingress system ..."
              << std::endl;
    entries = store_reg_.Query("ingress");
  }

  std::string remote = entries.front().protocol + "://" +
                       entries.front().address + ":" +
                       std::to_string(entries.front().port);
  std::cout << "[storage] Found available ingress: " << remote
            << " - connecting ..." << std::endl;
  int res = static_cast<int>(Ingress(remote));
  if (!res) {
    return res;
  }

  std::cout << "[storage] Preparing flow cache ... " << std::endl;
  struct stat cache_info;
  res = stat(config.store_path.c_str(), &cache_info);
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
      std::cout << "[storage] Attempting to create cache: " << config.store_path
                << std::endl;
      mkdir(config.store_path.c_str(), S_IRWXU);
      res = stat(config.store_path.c_str(), &cache_info);
      if (res) {
        perror("Failed to create file cache - ");
        return false;
      }
    }
  }

  std::cout << "[storage] Spinning up worker thread ..." << std::endl;
  worker_sock_ = new zmq::socket_t(*ctx_, zmq::socket_type::pair);
  worker_sock_->bind(config.worker);
  worker_.Init(ctx_, config);
  worker_.Launch();
  zmq::message_t tmsg;
  worker_sock_->recv(&tmsg);
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
  ingress_sock_ = new zmq::socket_t(*ctx_, zmq::socket_type::pull);
  bool success = true;
  try {
    ingress_sock_->connect(remote);
  } catch (error_t ex) {
    success = false;
  }

  return success;
}

void hdtn::Storage::Update() {
  zmq::pollitem_t items[] = {{ingress_sock_->handle(), 0, ZMQ_POLLIN, 0},
                             {telemetry_sock_->handle(), 0, ZMQ_POLLIN, 0}};
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
  telemetry_sock_->recv(&message);
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
  ingress_sock_->recv(&hdr);
  stats_.in_bytes += hdr.size();
  ++stats_.in_msg;

  if (hdr.size() < sizeof(hdtn::CommonHdr)) {
    std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
    return;
  }
  hdtn::CommonHdr *common = (hdtn::CommonHdr *)hdr.data();
  hdtn::BlockHdr *block = (hdtn::BlockHdr *)common;
  switch (common->type) {
    case HDTN_MSGTYPE_STORE:
      ingress_sock_->recv(&message);
      worker_sock_->send(hdr.data(), hdr.size(), ZMQ_MORE);
      stats_.in_bytes += message.size();
      worker_sock_->send(message.data(), message.size(), 0);
      break;
  }
}

void hdtn::Storage::Release(uint32_t flow, uint64_t rate, uint64_t duration) {}
