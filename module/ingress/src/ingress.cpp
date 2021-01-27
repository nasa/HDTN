#include "ingress.h"
#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <fstream>
#include <iostream>
#include "logging.hpp"
#include "message.hpp"
#include "reg.hpp"

#define BP_INGRESS_TELEM_FREQ (0.10)
#define INGRESS_PORT (4556)

using namespace hdtn;
using namespace std;
using namespace zmq;
static BpIngress ingress;

static void s_signal_handler(int signal_value) {
  // s_interrupted = 1;
  ofstream output;
  std::string current_date = Datetime();
  output.open("ingress-" + current_date);
  output << "Elapsed, Bundle Count (M),Rate (Mbps),Bundles/sec, Bundle Data "
            "(MB)\n";
  double rate = 8 * ((ingress.bundle_data_ / (double)(1024 * 1024)) / ingress.elapsed_);
  output << ingress.elapsed_ << "," << ingress.bundle_count_ / 1000000.0f << ","
         << rate << "," << ingress.bundle_count_ / ingress.elapsed_ << ", "
         << ingress.bundle_data_ / (double)(1024 * 1024) << "\n";
  output.close();
  exit(EXIT_SUCCESS);
}

static void s_catch_signals(void) {
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
}

int main(int argc, char *argv[]) {
  ingress.Init(BP_INGRESS_TYPE_UDP);
  uint64_t last_time = 0;
  uint64_t curr_time = 0;
  // finish registration stuff -ingress will find out what egress services have
  // registered
  HdtnRegsvr regsvr;
  regsvr.Init(HDTN_REG_SERVER_PATH, "ingress", 10100, "PUSH");
  regsvr.Reg();
  hdtn_entries res = regsvr.Query();
  for (auto entry : res) {
    std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
  }
  s_catch_signals();
  printf("Announcing presence of ingress engine ...\n");

  ingress.Netstart(INGRESS_PORT);
  int count = 0;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
  printf("Start: +%f\n", start);
  while (true) {
    curr_time = time(0);
    gettimeofday(&tv, NULL);
    ingress.elapsed_ = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    ingress.elapsed_ -= start;
    count = ingress.Update();
    ingress.Process(count);
    last_time = curr_time;
  }

  exit(EXIT_SUCCESS);
}
