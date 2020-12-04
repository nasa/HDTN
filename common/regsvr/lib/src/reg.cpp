#include "reg.hpp"

#include <iostream>

#include "json.hpp"

#define HDTN_REGSTR "HDTN/1.0 REGISTER"
#define HDTN_DEREGSTR "HDTN/1.0 DEREGISTER"
#define HDTN_QUERY "HDTN/1.0 QUERY"

void hdtn::hdtn_regsvr::init(std::string target, std::string svc, uint16_t port, std::string mode) {
    _zmq_ctx = new zmq::context_t;
    _zmq_sock = new zmq::socket_t(*_zmq_ctx, zmq::socket_type::req);
    char tbuf[255];
    memset(tbuf, 0, 255);
    snprintf(tbuf, 255, "%s:%d:%s", svc.c_str(), port, mode.c_str());
    _zmq_sock->setsockopt(ZMQ_IDENTITY, (void *)tbuf, target.size());
    _zmq_sock->connect(target);
}

bool hdtn::hdtn_regsvr::reg() {
    _zmq_sock->send(HDTN_REGSTR, strlen(HDTN_REGSTR), 0);
    zmq::message_t msg;
    _zmq_sock->recv(&msg, 0);

    return true;
}

bool hdtn::hdtn_regsvr::dereg() {
    _zmq_sock->send(HDTN_DEREGSTR, strlen(HDTN_DEREGSTR), 0);
    zmq::message_t msg;
    _zmq_sock->recv(&msg, 0);
    return true;
}

hdtn::hdtn_entries hdtn::hdtn_regsvr::query(std::string type) {
    std::string q_str = HDTN_QUERY;
    if ("" != type) {
        q_str += " " + type;
    }

    _zmq_sock->send(q_str.c_str(), strlen(q_str.c_str()), 0);
    hdtn::hdtn_entries result;
    zmq::message_t msg;
    _zmq_sock->recv(&msg, 0);
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
