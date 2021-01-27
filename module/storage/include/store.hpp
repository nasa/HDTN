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

struct ScheduleEvent {
  double ts;  // time at which this event will trigger
  uint32_t type;
  uint32_t flow;
  uint64_t rate;      //  bytes / sec
  uint64_t duration;  //  msec
};

typedef std::vector<ScheduleEvent> ReleaseList;

class Scheduler {
 public:
  void Init();
  void Add(CscheduleHdr *hdr);
  ScheduleEvent *Next();

 private:
  ReleaseList schedule_;
};

struct StorageConfig {
  StorageConfig()
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
  void Init(zmq::context_t *ctx, StorageConfig config);
  void Launch();
  void *Execute(void *arg);
  pthread_t *Thread() { return &thread_; }
  void Write(hdtn::BlockHdr *hdr, zmq::message_t *message);

 private:
  zmq::context_t *ctx_;
  pthread_t thread_;
  std::string root_;
  std::string queue_;
  char *out_buf_;
  hdtn::FlowStore store_;
};

class Storage {
 public:
  bool Init(StorageConfig config);
  void Update();
  void Dispatch();
  void C2telem();
  void Release(uint32_t flow, uint64_t rate, uint64_t duration);
  bool Ingress(std::string remote);
  StorageStats *Stats() { return &stats_; }

 private:
  zmq::context_t *ctx_;
  zmq::socket_t *ingress_sock_;
  HdtnRegsvr store_reg_;
  HdtnRegsvr telem_reg_;
  uint16_t port_;
  zmq::socket_t *egress_sock_;
  zmq::socket_t *worker_sock_;
  zmq::socket_t *telemetry_sock_;
  StorageWorker worker_;
  StorageStats stats_;
};
}  // namespace hdtn

#endif
