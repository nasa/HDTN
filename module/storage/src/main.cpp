#include <sys/time.h>
#include <iostream>
#include "store.hpp"

static uint64_t last_bytes;
static uint64_t last_count;

int main(int argc, char *argv[]) {
  double last = 0.0;
  timeval tv;
  gettimeofday(&tv, NULL);
  last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
  hdtn::StorageConfig config;
  config.regsvr = HDTN_REG_SERVER_PATH;
  config.local = HDTN_RELEASE_PATH;
  // should add storage path to config file and check if it exists
  config.store_path = "/home/hdtn/hdtn.store";
  hdtn::Storage store;
  std::cout << "[store] Initializing storage manager ..." << std::endl;
  if (!store.Init(config)) {
    return -1;
  }
  while (true) {
    store.Update();
    gettimeofday(&tv, NULL);
    double curr = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    if (curr - last > 1) {
      last = curr;
      hdtn::StorageStats *stats = store.Stats();
      uint64_t cbytes = stats->in_bytes - last_bytes;
      uint64_t ccount = stats->in_msg - last_count;
      last_bytes = stats->in_bytes;
      last_count = stats->in_msg;

      printf("[store] Received: %d msg / %0.2f MB\n", ccount,
             cbytes / (1024.0 * 1024.0));
    }
  }

  return 0;
}
