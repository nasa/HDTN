#ifndef _HDTN3_REG_H
#define _HDTN3_REG_H

#include <list>
#include <string>
#include <zmq.hpp>

namespace hdtn {

struct hdtn_entry {
  std::string protocol;
  std::string address;
  std::string type;
  uint16_t port;
  std::string mode;
};

typedef std::list<hdtn_entry> hdtn_entries;

class HdtnRegsvr {
 public:
  void Init(std::string target, std::string svc, uint16_t port,
            std::string mode);
  bool Reg();
  bool Dereg();
  hdtn::hdtn_entries Query(std::string target = "");

 private:
  zmq::context_t *zmq_ctx_;
  zmq::socket_t *zmq_sock_;
  std::string type_;
  std::string mode_;
  uint16_t port_;
};

};  // namespace hdtn

#endif
