#include <math.h>
#include <iostream>
#include "blosc.h"
#include "message.hpp"
#include "store.hpp"

static void *LaunchWrapper(void *arg) {
  hdtn::StorageWorker *worker = (hdtn::StorageWorker *)arg;
  return worker->Execute(NULL);
}

hdtn::StorageWorker::~StorageWorker() { free(out_buf_); }

void hdtn::StorageWorker::Init(zmq::context_t *ctx, StorageConfig config) {
  ctx_ = ctx;
  root_ = config.store_path;
  queue_ = config.worker;
}

void *hdtn::StorageWorker::Execute(void *arg) {
  zmq::message_t rhdr;
  zmq::message_t rmsg;
  std::cout << "[storage-worker] Worker thread starting up." << std::endl;
  out_buf_ = (char *)malloc(HDTN_BLOSC_MAXBLOCKSZ);

  zmq::socket_t worker_sock(*ctx_, zmq::socket_type::pair);
  worker_sock.connect(queue_.c_str());
  std::cout << "[storage-workectx_nitializing flow store ... " << std::endl;
  CommonHdr startup_notify = {HDTN_MSGTYPE_IOK, 0};
  if (!store_.Init(root_)) {
    startup_notify.type = HDTN_MSGTYPE_IABORT;
    worker_sock.send(&startup_notify, sizeof(CommonHdr));
    return NULL;
  }
  worker_sock.send(&startup_notify, sizeof(CommonHdr));
  std::cout << "[storage-worker] Notified parent that startup is complete."
            << std::endl;
  while (true) {
    worker_sock.recv(&rhdr);
    if (rhdr.size() < sizeof(hdtn::CommonHdr)) {
      std::cerr
          << "[storage-worker] Invalid message format - header size too small ("
          << rhdr.size() << ")" << std::endl;
      continue;
    }
    hdtn::CommonHdr *common = (hdtn::CommonHdr *)rhdr.data();
    if (common->type == HDTN_MSGTYPE_STORE) {
      worker_sock.recv(&rmsg);
      hdtn::BlockHdr *block = (hdtn::BlockHdr *)rhdr.data();
      if (rhdr.size() != sizeof(hdtn::BlockHdr)) {
        std::cerr << "[storage-worker] Invalid message format - header size "
                     "mismatch ("
                  << rhdr.size() << ")" << std::endl;
      }
      Write(block, &rmsg);
    }
  }
  return NULL;
}

void hdtn::StorageWorker::Write(hdtn::BlockHdr *hdr, zmq::message_t *message) {
  // std::cerr << "[storage-worker] Received chunk of size " << rmsg.size() <<
  // std::endl;
  uint64_t chunks = ceil(message->size() / (double)HDTN_BLOSC_MAXBLOCKSZ);
  for (int i = 0; i < chunks; ++i) {
    int res = blosc_compress_ctx(9, 0, 4, message->size(), message->data(),
                                 out_buf_, HDTN_BLOSC_MAXBLOCKSZ, "lz4", 0, 1);
    store_.Write(hdr->flow_id, out_buf_, res);
    // std::cerr << "[storage-worker] Appending block (" << rmsg.size() << " raw
    // / " << res
    //<< " compressed / ratio=" << ((double)(res))/rmsg.size() << ")" <<
    //std::endl;
  }
}

void hdtn::StorageWorker::Launch() {
  std::cout << "[storage-worker] Launching worker thread ..." << std::endl;
  pthread_create(&thread_, NULL, LaunchWrapper, this);
}
