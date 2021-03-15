#ifndef _HDTN3_REG_H
#define _HDTN3_REG_H

#include <list>
#include <string>
#include <zmq.hpp>
#include <boost/shared_ptr.hpp>
#include "JsonSerializable.h"

namespace hdtn {

struct HdtnEntry {
  std::string protocol;
  std::string address;
  std::string type;
  uint16_t port;
  std::string mode;

  //a default constructor: X()
  HdtnEntry();

  //a destructor: ~X()
  ~HdtnEntry();

  //a copy constructor: X(const X&)
  HdtnEntry(const HdtnEntry& o);

  //a move constructor: X(X&&)
  HdtnEntry(HdtnEntry&& o);

  //a copy assignment: operator=(const X&)
  HdtnEntry& operator=(const HdtnEntry& o);

  //a move assignment: operator=(X&&)
  HdtnEntry& operator=(HdtnEntry&& o);

  bool operator==(const HdtnEntry & o) const;
};

typedef std::list<HdtnEntry> HdtnEntryList_t;

class HdtnEntries;
typedef boost::shared_ptr<HdtnEntries> HdtnEntries_ptr;

class HdtnEntries : public JsonSerializable {


public:
    HdtnEntries();
    ~HdtnEntries();

    bool operator==(const HdtnEntries & other) const;

    static HdtnEntries_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    static HdtnEntries_ptr CreateFromJson(const std::string & jsonString);
    static HdtnEntries_ptr CreateFromJsonFile(const std::string & jsonFileName);
    virtual boost::property_tree::ptree GetNewPropertyTree() const;
    virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt);

    void AddEntry(const std::string & protocol, const std::string & address, const std::string & type, uint16_t port, const std::string & mode);
public:

    HdtnEntryList_t m_hdtnEntryList;
};

class HdtnRegsvr {
 public:
  void Init(std::string target, std::string svc, uint16_t port,
            std::string mode);
  bool Reg();
  bool Dereg();
  HdtnEntries_ptr Query(const std::string & type = "");

 private:
    std::unique_ptr<zmq::context_t> m_zmqCtx;
    std::unique_ptr<zmq::socket_t> m_zmqSock;
    std::string m_type;
    std::string m_mode;
    uint16_t m_port;
};

};  // namespace hdtn

#endif
