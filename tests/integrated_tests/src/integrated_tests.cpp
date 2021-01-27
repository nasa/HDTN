#include <arpa/inet.h>
#include <codec/bpv6.h>
#include <egress.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <ingress.h>
#include <sys/time.h>
#include <unistd.h>
#include <util/tsc.h>
#include <fstream>
#include <iostream>
#include <reg.hpp>
#include <store.hpp>
#include <thread>
#include <zmq.hpp>

// Used for forking python process
#include <signal.h> /* for SIGTERM, SIGKILL */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* for pid_t            */
#include <sys/wait.h>  /* for waitpid          */
#include <unistd.h>    /* for fork, exec, kill */

// Prototypes
std::string GetEnv(const std::string& var);
int RunBpgen();
int RunIngress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
int RunEgress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
bool IntegratedTest1();
pid_t SpawnPythonServer(void);
int KillProcess(pid_t processId);

volatile bool RUN_BPGEN = true;
volatile bool RUN_INGRESS = true;
volatile bool RUN_EGRESS = true;
volatile bool RUN_STORAGE = true;

// Create a test fixture.
class IntegratedTestsFixture : public testing::Test {
 public:
  IntegratedTestsFixture();
  ~IntegratedTestsFixture();
  void SetUp() override;     // This is called after constructor.
  void TearDown() override;  // This is called before destructor.
};

IntegratedTestsFixture::IntegratedTestsFixture() {
  //    std::cout << "Called IntegratedTests1Fixture::IntegratedTests1Fixture()"
  //    << std::endl;
}

IntegratedTestsFixture::~IntegratedTestsFixture() {
  //    std::cout << "Called
  //    IntegratedTests1Fixture::~IntegratedTests1Fixture()" << std::endl;
}

void IntegratedTestsFixture::SetUp() {
  //    std::cout << "IntegratedTests1Fixture::SetUp called\n";
}

void IntegratedTestsFixture::TearDown() {
  //    std::cout << "IntegratedTests1Fixture::TearDown called\n";
}

TEST_F(IntegratedTestsFixture, IntegratedTest1) {
  bool result = IntegratedTest1();
  EXPECT_EQ(true, result);
}

bool IntegratedTest1() {
  std::cout << "Running Integrated Tests. " << std::endl << std::flush;

  //    pid_t pidPythonServer = spawnPythonServer();
  //    if (pidPythonServer == -1) {
  //        fprintf(stderr, "failed to fork child process\n");
  //        return -1;
  //    }
  //    printf("Spawned python server with pid %d\n", pidPythonServer);
  //    fflush(stdout);
  //    sleep(1);

  uint64_t bundle_count_ingress = 0;
  uint64_t bundle_data_ingress = 0;
  std::thread thread_ingress(RunIngress, &bundle_count_ingress,
                            &bundle_data_ingress);
  //    sleep(1);
  uint64_t bundle_count_egress = 0;
  uint64_t bundle_data_egress = 0;
  std::thread thread_egress(RunEgress, &bundle_count_egress, &bundle_data_egress);
  sleep(1);
  std::thread thread_bpgen(RunBpgen);
  sleep(3);
  RUN_BPGEN = false;
  thread_bpgen.join();
  sleep(2);
  RUN_INGRESS = false;
  std::cout << "Before threadIngress.join(). " << std::endl << std::flush;
  thread_ingress.join();
  std::cout << "After threadIngress.join(). " << std::endl << std::flush;
  sleep(1);
  RUN_EGRESS = false;
  std::cout << "Before threadEgress.join(). " << std::endl << std::flush;
  thread_egress.join();
  std::cout << "After threadEgress.join(). " << std::endl << std::flush;

  //    killProcess(pidPythonServer);

  std::cout << "End Integrated Tests. " << std::endl << std::flush;
  sleep(3);
  std::cout << "bundleCountIngress: " << bundle_count_egress
            << " , bundleDataIngress: " << bundle_data_ingress << std::endl
            << std::flush;
  std::cout << "bundleCountEgress: " << bundle_count_egress
            << " , bundleDataEgress: " << bundle_data_egress << std::endl
            << std::flush;
  if ((bundle_count_egress == bundle_count_egress) &&
      (bundle_data_ingress == bundle_data_egress)) {
    return true;
  }
  return false;
}

int RunBpgen() {
  std::cout << "Start runBpgen ... " << std::endl << std::flush;
  struct BpgenHdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
  };
  int bp_msg_bufsz = 65536;
  int bp_msg_nbuf = 32;
  struct mmsghdr* msgbuf;
  uint64_t bundle_count = 0;
  uint64_t bundle_data = 0;
  uint64_t raw_data = 0;
  uint64_t rate = 50;
  std::string target = "127.0.0.1";
  struct sockaddr_in servaddr;
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  int source_node = 1;
  int dest_node = 1;
  int port = 4556;
  size_t gen_sz = 1500;
  ssize_t res;
  printf("Generating bundles of size %d\n", (int)gen_sz);
  if (rate) {
    printf("Generating up to %d bundles / second.\n", (int)rate);
  }
  printf("Bundles will be destinated for %s:%d\n", target.c_str(), port);
  fflush(stdout);
  char* data_buffer[gen_sz];
  memset(data_buffer, 0, gen_sz);
  BpgenHdr* hdr = (BpgenHdr*)data_buffer;
  uint64_t last_time = 0;
  uint64_t curr_time = 0;
  uint64_t seq = 0;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(target.c_str());
  servaddr.sin_port = htons(port);
  msgbuf = (mmsghdr*)malloc(sizeof(struct mmsghdr) * bp_msg_nbuf);
  memset(msgbuf, 0, sizeof(struct mmsghdr) * bp_msg_nbuf);
  struct iovec* io = (iovec*)malloc(sizeof(struct iovec) * bp_msg_nbuf);
  memset(io, 0, sizeof(struct iovec) * bp_msg_nbuf);
  char** tmp = (char**)malloc(bp_msg_nbuf * sizeof(char*));
  int i = 0;
  for (i = 0; i < bp_msg_nbuf; ++i) {
    tmp[i] = (char*)malloc(bp_msg_bufsz);
    memset(tmp[i], 0x0, bp_msg_bufsz);
  }
  for (i = 0; i < bp_msg_nbuf; ++i) {
    msgbuf[i].msg_hdr.msg_iov = &io[i];
    msgbuf[i].msg_hdr.msg_iovlen = 1;
    msgbuf[i].msg_hdr.msg_iov->iov_base = tmp[i];
    msgbuf[i].msg_hdr.msg_iov->iov_len = bp_msg_bufsz;
    msgbuf[i].msg_hdr.msg_name = (void*)&servaddr;
    msgbuf[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
  }
  printf("Entering run state ...\n");
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
  printf("Start: +%f\n", start);
  fflush(stdout);
  uint64_t tsc_total = 0;
  double sval = 0.0;
  if (rate) {
    sval = 1000000.0 / rate;  // sleep val in usec
    sval *= bp_msg_nbuf;
    printf("Sleeping for %f usec between bursts\n", sval);
    fflush(stdout);
  }
  uint64_t bseq = 0;
  uint64_t total_bundle_count = 0;
  uint64_t totalSize = 0;
  while (RUN_BPGEN) {
    for (int idx = 0; idx < bp_msg_nbuf; ++idx) {
      char* curr_buf = (char*)(msgbuf[idx].msg_hdr.msg_iov->iov_base);
      curr_time = time(0);
      if (curr_time == last_time) {
        ++seq;
      } else {
        gettimeofday(&tv, NULL);
        double elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        elapsed -= start;
        start = start + elapsed;
        bundle_count = 0;
        bundle_data = 0;
        raw_data = 0;
        tsc_total = 0;
        seq = 0;
      }
      last_time = curr_time;
      bpv6_primary_block primary;
      memset(&primary, 0, sizeof(bpv6_primary_block));
      primary.version = 6;
      bpv6_canonical_block block;
      memset(&block, 0, sizeof(bpv6_canonical_block));
      uint64_t bundle_length = 0;
      primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) |
                      bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON |
                                             BPV6_BUNDLEFLAG_NOFRAGMENT);
      primary.src_node = source_node;
      primary.src_svc = 1;
      primary.dst_node = dest_node;
      primary.dst_svc = 1;
      primary.creation = (uint64_t)bpv6_unix_to_5050(curr_time);
      primary.sequence = seq;
      uint64_t tsc_start = rdtsc();
      bundle_length =
          bpv6_primary_block_encode(&primary, curr_buf, 0, bp_msg_bufsz);
      tsc_total += rdtsc() - tsc_start;
      block.type = BPV6_BLOCKTYPE_PAYLOAD;
      block.flags = BPV6_BLOCKFLAG_LAST_BLOCK;
      block.length = gen_sz;
      tsc_start = rdtsc();
      bundle_length += bpv6_canonical_block_encode(&block, curr_buf,
                                                   bundle_length, bp_msg_bufsz);
      tsc_total += rdtsc() - tsc_start;
      hdr->tsc = rdtsc();
      clock_gettime(CLOCK_REALTIME, &hdr->abstime);
      hdr->seq = bseq++;
      memcpy(curr_buf + bundle_length, data_buffer, gen_sz);
      bundle_length += gen_sz;
      msgbuf[idx].msg_hdr.msg_iov[0].iov_len = bundle_length;
      ++bundle_count;
      bundle_data += gen_sz;      // payload data
      raw_data += bundle_length;  // bundle overhead + payload data
    }  // End for(idx = 0; idx < BP_MSG_NBUF; ++idx) {
    res = sendmmsg(fd, msgbuf, bp_msg_nbuf, 0);
    if (res < 0) {
      perror("cannot send message");
    } else {
      totalSize += msgbuf->msg_len;
      total_bundle_count += bundle_count;
      std::cout << "In BPGEN, totalBundleCount: " << total_bundle_count
                << " , totalSize: " << totalSize << std::endl
                << std::flush;
    }
    if (rate) {
      usleep((uint64_t)sval);
    }
  }
  close(fd);
  std::cout << "End runBpgen ... " << std::endl << std::flush;
  return 0;
}

int RunIngress(uint64_t* ptr_bundle_count, uint64_t* ptr_bundle_data) {
  std::cout << "Start runIngress ... " << std::endl << std::flush;
  int ingress_port = 4556;
  hdtn::BpIngress ingress;
  ingress.Init(BP_INGRESS_TYPE_UDP);
  uint64_t last_time = 0;
  uint64_t curr_time = 0;
  // finish registration stuff -ingress will find out what egress services have
  // registered
  hdtn::HdtnRegsvr regsvr;
  regsvr.Init("tcp://127.0.0.1:10140", "ingress", 10149, "PUSH");
  regsvr.Reg();
  hdtn::hdtn_entries res = regsvr.Query();
  for (auto entry : res) {
    std::cout << entry.address << ":" << entry.port << ":" << entry.mode
              << std::endl;
  }
  printf("Announcing presence of ingress engine ...\n");
  fflush(stdout);
  ingress.Netstart(ingress_port);
  int count = 0;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
  printf("Start: +%f\n", start);
  fflush(stdout);
  while (RUN_INGRESS) {
    curr_time = time(0);
    gettimeofday(&tv, NULL);
    ingress.elapsed_ = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    ingress.elapsed_ -= start;
    // count = ingress.update();
    count = ingress.Update(0.5);  // Use timeout so call does not indefinitely
                                  // block.  Units are seconds
    if (count > 0) {
      ingress.Process(count);
    }
    last_time = curr_time;
  }
  std::cout << "End runIngress ... " << std::endl << std::flush;
  *ptr_bundle_count = ingress.bundle_count_;
  *ptr_bundle_data = ingress.bundle_data_;
  return 0;
}

int RunEgress(uint64_t* ptr_bundle_count, uint64_t* ptr_bundle_data) {
  std::cout << "Start runEgress ... " << std::endl << std::flush;
  uint64_t message_count = 0;
  double elapsed = 0;
  hdtn::HegrManager egress;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
  printf("Start Egress: +%f\n", start);
  fflush(stdout);
  // finish registration stuff - egress should register, ingress will query
  hdtn::HdtnRegsvr regsvr;
  regsvr.Init("tcp://127.0.0.1:10140", "egress", 10149, "PULL");
  regsvr.Reg();
  hdtn::hdtn_entries res = regsvr.Query();
  for (auto entry : res) {
    std::cout << entry.address << ":" << entry.port << ":" << entry.mode
              << std::endl
              << std::flush;
  }
  zmq::context_t* zmq_ctx;
  zmq::socket_t* zmq_sock;
  zmq_ctx = new zmq::context_t;
  zmq_sock = new zmq::socket_t(*zmq_ctx, zmq::socket_type::pull);
  zmq_sock->connect("tcp://127.0.0.1:10149");
  egress.Init();
  int entry_status;
  entry_status = egress.Add(1, HEGR_FLAG_UDP, "127.0.0.1", 4557);
  if (!entry_status) {
    return 0;  // error message prints in add function
  }
  printf("Announcing presence of egress ...\n");
  fflush(stdout);
  for (int i = 0; i < 8; ++i) {
    egress.Up(i);
  }
  int bundle_size = 0;
  // JCF, set timeout
  // Use a form of receive that times out so we can terminate cleanly.
  int timeout = 250;  // milliseconds
  zmq_sock->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(int));
  while (RUN_EGRESS) {
    gettimeofday(&tv, NULL);
    elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    elapsed -= start;
    zmq::message_t hdr;
    zmq::message_t message;
    // JCF
    // Use a form of receive that times out so we can terminate cleanly.  If no
    // message was received after timeout go back to top of loop
    // std::cout << "In runEgress, before recv. " << std::endl << std::flush;
    bool retValue = zmq_sock->recv(&hdr);
    // std::cout << "In runEgress, after recv. retValue = " << retValue << " ,
    // hdr.size() = " << hdr.size() << std::endl << std::flush;
    if (!retValue) {
      continue;
    }
    message_count++;
    char bundle[HMSG_MSG_MAX];
    if (hdr.size() < sizeof(hdtn::CommonHdr)) {
      std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl
                << std::flush;
      return -1;
    }
    zmq_sock->recv(&message);
    bundle_size = message.size();
    memcpy(bundle, message.data(), bundle_size);
    (*ptr_bundle_data) += bundle_size;
    (*ptr_bundle_count)++;
  }
  // Clean up resources
  zmq_sock->close();
  delete zmq_sock;
  delete zmq_ctx;
  return 0;
}

pid_t SpawnPythonServer(void) {
  int outfd = open("1.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (!outfd) {
    perror("open");
    return EXIT_FAILURE;
  }

  // fork a child process, execute vlc, and return it's pid. Returns -1 if fork
  // failed.
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return -1;
  }
  // when you call fork(), it creates two copies of your program: a parent, and
  // a child. You can tell them apart by the return value from fork().  If
  // fork() returns 0, this is is the child process.  If fork() returns
  // non-zero, we are the parent and the return value is the PID of the child
  // process. */
  if (pid == 0) {
    // this is the child process.  now we can call one of the exec family of
    // functions to execute VLC.  When you call exec, it replaces the currently
    // running process (the child process) with whatever we pass to exec.  So
    // our child process will now be running VLC.  exec() will never return
    // except in an error case, since it is now running the VLC code and not our
    // code.
    std::string command_arg =
        GetEnv("HDTN_SOURCE_ROOT") + "/common/regsvr/main.py";
    std::cout << "Running python3 " << command_arg << std::endl << std::flush;
    execlp("python3", command_arg.c_str(), (char*)NULL);
    std::cerr << "ERROR Running python3 " << command_arg << std::endl
              << std::flush;
    perror("python3");
    abort();

  } else {
    // parent, return the child's PID
    return pid;
  }
}

int KillProcess(pid_t process_id) {
  // kill will send the specified signal to the specified process. Send a TERM
  // signal to VLC, requesting that it terminate. If that doesn't work, we send
  // a KILL signal.  If that doesn't work, we give up.
  if (kill(process_id, SIGTERM) < 0) {
    perror("kill with SIGTERM");
    if (kill(process_id, SIGKILL) < 0) {
      perror("kill with SIGKILL");
    }
  }
  // This shows how we can get the exit status of our child process.  It will
  // wait for the the VLC process to exit, then grab it's return value
  int status = 0;
  waitpid(process_id, &status, 0);
  return status;
}

std::string GetEnv(const std::string& var) {
  const char* val = std::getenv(var.c_str());
  if (val == nullptr) {  // invalid to assign nullptr to std::string
    return "";
  } else {
    return val;
  }
}
