#ifndef _HDTN3_REG_H
#define _HDTN3_REG_H

#include <list>
#include <string>
#include <zmq.hpp>

namespace hdtn {

struct HdtnEntry {
  std::string protocol;
  std::string address;
  std::string type;
  uint16_t port;
  std::string mode;
};

typedef std::list<HdtnEntry> HdtnEntries;

class HdtnRegsvr {
 public:
  void Init(std::string target, std::string svc, uint16_t port,
            std::string mode);
  bool Reg();
  bool Dereg();
  hdtn::HdtnEntries Query(std::string target = "");

 private:
  zmq::context_t *m_zmqCtx;
  zmq::socket_t *m_zmqSock;
  std::string m_type;
  std::string m_mode;
  uint16_t m_port;
};

};  // namespace hdtn

#endif
