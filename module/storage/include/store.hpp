#ifndef _HDTN_STORE_H
#define _HDTN_STORE_H

#include <map>
#include <string>
#include <vector>
#include "cache.hpp"
#include "paths.hpp"
#include "reg.hpp"
#include "stats.hpp"
#include "zmq.hpp"

#define HDTN_STORAGE_TELEM_FLOWCOUNT (4)
#define HDTN_STORAGE_PORT_DEFAULT (10425)
#define HDTN_BLOSC_MAXBLOCKSZ (1 << 26)
#define HDTN_FLOWCOUNT_MAX (16777216)

namespace hdtn {

struct schedule_event {
  double ts;  // time at which this event will trigger
  uint32_t type;
  uint32_t flow;
  uint64_t rate;      //  bytes / sec
  uint64_t duration;  //  msec
};

typedef std::vector<schedule_event> release_list;

class Scheduler {
 public:
  void Init();
  void Add(cschedule_hdr *hdr);
  schedule_event *Next();

 private:
  release_list schedule_;
};

struct storage_config {
  storage_config()
      : telem(HDTN_STORAGE_TELEM_PATH), worker(HDTN_STORAGE_WORKER_PATH) {}

  /**
   * 0mq endpoint for registration server
   */
  std::string regsvr;
  /**
   * 0mq endpoint for storage service
   */
  std::string local;
  /**
   * Filesystem location for flow / data storage
   */
  std::string store_path;
  /**
   * 0mq endpoint for local telemetry service
   */
  std::string telem;
  /**
   * 0mq inproc endpoint for worker's use
   */
  std::string worker;
};

class StorageWorker {
 public:
  ~StorageWorker();
  void Init(zmq::context_t *ctx, storage_config config);
  void Launch();
  void *Execute(void *arg);
  pthread_t *thread() { return &_thread; }
  void Write(hdtn::block_hdr *hdr, zmq::message_t *message);

 private:
  zmq::context_t *ctx_;
  pthread_t _thread;
  std::string _root;
  std::string _queue;
  char *_out_buf;
  hdtn::flow_store _store;
};

class storage {
 public:
  bool init(storage_config config);
  void update();
  void dispatch();
  void c2telem();
  void release(uint32_t flow, uint64_t rate, uint64_t duration);
  bool ingress(std::string remote);
  storage_stats *stats() { return &_stats; }

 private:
  zmq::context_t *_ctx;
  zmq::socket_t *_ingress_sock;
  HdtnRegsvr _store_reg;
  HdtnRegsvr _telem_reg;
  uint16_t _port;
  zmq::socket_t *_egress_sock;
  zmq::socket_t *_worker_sock;
  zmq::socket_t *_telemetry_sock;
  StorageWorker _worker;

  storage_stats _stats;
};
}  // namespace hdtn

#endif
