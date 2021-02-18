#include "reg.hpp"
#include <iostream>
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <iostream>

#define HDTN_REGSTR "HDTN/1.0 REGISTER"
#define HDTN_DEREGSTR "HDTN/1.0 DEREGISTER"
#define HDTN_QUERY "HDTN/1.0 QUERY"

namespace hdtn {

//a default constructor: X()
HdtnEntry::HdtnEntry() { }

//a destructor: ~X()
HdtnEntry::~HdtnEntry() { }

//a copy constructor: X(const X&)
HdtnEntry::HdtnEntry(const HdtnEntry& o) : protocol(o.protocol), address(o.address), type(o.type), port(o.port), mode(o.mode) { }

//a move constructor: X(X&&)
HdtnEntry::HdtnEntry(HdtnEntry&& o) : protocol(std::move(o.protocol)), address(std::move(o.address)), type(std::move(o.type)), port(o.port), mode(std::move(o.mode)) { }

//a copy assignment: operator=(const X&)
HdtnEntry& HdtnEntry::operator=(const HdtnEntry& o) {
     protocol = o.protocol;
     address = o.address;
     type = o.type;
     port = o.port;
     mode = o.mode;
     return *this;
}

//a move assignment: operator=(X&&)
HdtnEntry& HdtnEntry::operator=(HdtnEntry&& o) {
    protocol = std::move(o.protocol);
    address = std::move(o.address);
    type = std::move(o.type);
    port = o.port;
    mode = std::move(o.mode);
    return *this;
}

bool HdtnEntry::operator==(const HdtnEntry & o) const {
    return (protocol == o.protocol) && (address == o.address) && (type == o.type) && (port == o.port) && (mode == o.mode);
}

void HdtnRegsvr::Init(std::string target, std::string svc, uint16_t port, std::string mode) {
    m_zmqSock = boost::shared_ptr<zmq::socket_t>(); //delete any existing
    m_zmqCtx = boost::make_shared<zmq::context_t>();
    m_zmqSock = boost::make_shared<zmq::socket_t>(*m_zmqCtx, zmq::socket_type::req);
    char tbuf[255];
    memset(tbuf, 0, 255);
    snprintf(tbuf, 255, "%s:%d:%s", svc.c_str(), port, mode.c_str());
    m_zmqSock->setsockopt(ZMQ_IDENTITY, (void *)tbuf, target.size());
    // Use a form of receive that times out so we can terminate cleanly.
    int timeout = 2000;  // milliseconds
    m_zmqSock->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(int));
    m_zmqSock->connect(target);
}

bool HdtnRegsvr::Reg() {
    m_zmqSock->send(HDTN_REGSTR, strlen(HDTN_REGSTR), 0);
    zmq::message_t msg;
    return m_zmqSock->recv(&msg, 0); //false if timeout
}

bool HdtnRegsvr::Dereg() {
    m_zmqSock->send(HDTN_DEREGSTR, strlen(HDTN_DEREGSTR), 0);
    zmq::message_t msg;
    return m_zmqSock->recv(&msg, 0); //false if timeout
}

HdtnEntries_ptr HdtnRegsvr::Query(const std::string & type) {
    std::string q_str = HDTN_QUERY;
    if ("" != type) {
        q_str += " " + type;
    }
    m_zmqSock->send(q_str.c_str(), strlen(q_str.c_str()), 0);
    hdtn::HdtnEntries result;
    zmq::message_t msg;
    if(!m_zmqSock->recv(&msg, 0)) { //timeout
        return HdtnEntries_ptr(); //null shared_ptr
    }
    size_t sz = msg.size();
    std::string q_res = std::string((char *)msg.data(), sz);
    //std::cout << q_res << std::endl;
    size_t payload_index = q_res.find("|");
    std::string payload = q_res.substr(payload_index + 2);
    return HdtnEntries::CreateFromJson(payload);
}





HdtnEntries::HdtnEntries() {}

HdtnEntries::~HdtnEntries() {}

void HdtnEntries::AddEntry(const std::string & protocol, const std::string & address, const std::string & type, uint16_t port, const std::string & mode) {
    HdtnEntry e;
    e.protocol = protocol;
    e.address = address;
    e.type = type;
    e.port = port;
    e.mode = mode;
    m_hdtnEntryList.push_back(std::move(e));
}

bool HdtnEntries::operator==(const HdtnEntries & other) const {
    return (m_hdtnEntryList == other.m_hdtnEntryList);
}

bool HdtnEntries::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {

    const boost::property_tree::ptree & hdtnEntryListPt = pt.get_child("hdtnEntryList", boost::property_tree::ptree()); //non-throw version
    m_hdtnEntryList.clear();
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & hdtnEntryPt, hdtnEntryListPt) {
        HdtnEntry e;
        e.protocol = hdtnEntryPt.second.get<std::string>("protocol", ""); //non-throw version
        e.address = hdtnEntryPt.second.get<std::string>("address", ""); //non-throw version
        e.type = hdtnEntryPt.second.get<std::string>("type", ""); //non-throw version
        e.port = hdtnEntryPt.second.get<uint16_t>("port", 0); //non-throw version
        e.mode = hdtnEntryPt.second.get<std::string>("mode", ""); //non-throw version
        m_hdtnEntryList.push_back(std::move(e));
    }

    return true;
}

HdtnEntries_ptr HdtnEntries::CreateFromJson(const std::string & jsonString) {
    try {
        return HdtnEntries::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonString(jsonString));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In HdtnEntries::CreateFromJson. Error: " + std::string(e.what());
        std::cerr << message << std::endl;
    }

    return HdtnEntries_ptr(); //NULL
}

HdtnEntries_ptr HdtnEntries::CreateFromJsonFile(const std::string & jsonFileName) {
    try {
        return HdtnEntries::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonFile(jsonFileName));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In HdtnEntries::CreateFromJsonFile. Error: " + std::string(e.what());
        std::cerr << message << std::endl;
    }

    return HdtnEntries_ptr(); //NULL
}

HdtnEntries_ptr HdtnEntries::CreateFromPtree(const boost::property_tree::ptree & pt) {

    HdtnEntries_ptr ptrHdtnEntries = boost::make_shared<HdtnEntries>();
    if (!ptrHdtnEntries->SetValuesFromPropertyTree(pt)) {
        ptrHdtnEntries = HdtnEntries_ptr(); //failed, so delete and set it NULL
    }
    return ptrHdtnEntries;
}

boost::property_tree::ptree HdtnEntries::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    boost::property_tree::ptree & hdtnEntryListPt = pt.put_child("hdtnEntryList", m_hdtnEntryList.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (HdtnEntryList_t::const_iterator it = m_hdtnEntryList.cbegin(); it != m_hdtnEntryList.cend(); ++it) {
        const HdtnEntry & entry = *it;
        boost::property_tree::ptree & entryPt = (hdtnEntryListPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
        entryPt.put("protocol", entry.protocol);
        entryPt.put("address", entry.address);
        entryPt.put("type", entry.type);
        entryPt.put("port", entry.port);
        entryPt.put("mode", entry.mode);
    }

    return pt;
}

} //end namespace hdtn
