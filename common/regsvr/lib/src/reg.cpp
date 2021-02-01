#include "reg.hpp"

#include <iostream>

#include "json.hpp"

#define HDTN_REGSTR "HDTN/1.0 REGISTER"
#define HDTN_DEREGSTR "HDTN/1.0 DEREGISTER"
#define HDTN_QUERY "HDTN/1.0 QUERY"

void hdtn::HdtnRegsvr::Init(std::string target, std::string svc, uint16_t port, std::string mode) {
    m_zmqCtx = new zmq::context_t;
    m_zmqSock = new zmq::socket_t(*m_zmqCtx, zmq::socket_type::req);
    char tbuf[255];
    memset(tbuf, 0, 255);
    snprintf(tbuf, 255, "%s:%d:%s", svc.c_str(), port, mode.c_str());
    m_zmqSock->setsockopt(ZMQ_IDENTITY, (void *)tbuf, target.size());
    m_zmqSock->connect(target);
}

bool hdtn::HdtnRegsvr::Reg() {
    m_zmqSock->send(HDTN_REGSTR, strlen(HDTN_REGSTR), 0);
    zmq::message_t msg;
    m_zmqSock->recv(&msg, 0);
    return true;
}

bool hdtn::HdtnRegsvr::Dereg() {
    m_zmqSock->send(HDTN_DEREGSTR, strlen(HDTN_DEREGSTR), 0);
    zmq::message_t msg;
    m_zmqSock->recv(&msg, 0);
    return true;
}

hdtn::HdtnEntries hdtn::HdtnRegsvr::Query(std::string type) {
    std::string q_str = HDTN_QUERY;
    if ("" != type) {
        q_str += " " + type;
    }
    m_zmqSock->send(q_str.c_str(), strlen(q_str.c_str()), 0);
    hdtn::HdtnEntries result;
    zmq::message_t msg;
    m_zmqSock->recv(&msg, 0);
    size_t sz = msg.size();
    std::string q_res = std::string((char *)msg.data(), sz);
    size_t payload_index = q_res.find("|");
    std::string payload = q_res.substr(payload_index + 2);
    nlohmann::json json;
    auto res = json.parse(payload);
    for (auto element : res.items()) {
        HdtnEntry entry;
        entry.address = std::string(element.value()["address"]);
        entry.mode = std::string(element.value()["mode"]);
        entry.port = atoi(std::string(element.value()["port"]).c_str());
        entry.protocol = std::string(element.value()["protocol"]);
        entry.type = std::string(element.value()["type"]);
        result.push_back(entry);
    }
    return result;
}
