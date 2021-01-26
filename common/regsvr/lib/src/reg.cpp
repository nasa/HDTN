#include "reg.hpp"
#include <iostream>
#include "json.hpp"

#define HDTN_REGSTR "HDTN/1.0 REGISTER"
#define HDTN_DEREGSTR "HDTN/1.0 DEREGISTER"
#define HDTN_QUERY "HDTN/1.0 QUERY"

void hdtn::HdtnRegsvr::Init(std::string target, std::string svc, uint16_t port,
                             std::string mode) {
  zmq_ctx_ = new zmq::context_t;
  zmq_sock_ = new zmq::socket_t(*zmq_ctx_, zmq::socket_type::req);
  char tbuf[255];
  memset(tbuf, 0, 255);
  snprintf(tbuf, 255, "%s:%d:%s", svc.c_str(), port, mode.c_str());
  zmq_sock_->setsockopt(ZMQ_IDENTITY, (void *)tbuf, target.size());
  zmq_sock_->connect(target);
}

bool hdtn::HdtnRegsvr::Reg() {
  zmq_sock_->send(HDTN_REGSTR, strlen(HDTN_REGSTR), 0);
  zmq::message_t msg;
  zmq_sock_->recv(&msg, 0);
  return true;
}

bool hdtn::HdtnRegsvr::Dereg() {
  zmq_sock_->send(HDTN_DEREGSTR, strlen(HDTN_DEREGSTR), 0);
  zmq::message_t msg;
  zmq_sock_->recv(&msg, 0);
  return true;
}

hdtn::hdtn_entries hdtn::HdtnRegsvr::Query(std::string type) {
  std::string q_str = HDTN_QUERY;
  if ("" != type) {
    q_str += " " + type;
  }
  zmq_sock_->send(q_str.c_str(), strlen(q_str.c_str()), 0);
  hdtn::hdtn_entries result;
  zmq::message_t msg;
  zmq_sock_->recv(&msg, 0);
  size_t sz = msg.size();
  std::string q_res = std::string((char *)msg.data(), sz);
  size_t payload_index = q_res.find("|");
  std::string payload = q_res.substr(payload_index + 2);
  nlohmann::json json;
  auto res = json.parse(payload);
  for (auto element : res.items()) {
    hdtn_entry entry;
    entry.address = std::string(element.value()["address"]);
    entry.mode = std::string(element.value()["mode"]);
    entry.port = atoi(std::string(element.value()["port"]).c_str());
    entry.protocol = std::string(element.value()["protocol"]);
    entry.type = std::string(element.value()["type"]);
    result.push_back(entry);
  }
  return result;
}
