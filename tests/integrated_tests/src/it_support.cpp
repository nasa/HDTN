#include <iostream>
#include <fstream>
#include <gtest/gtest.h>
#include <zmq.hpp>
#include <arpa/inet.h>
#include <sys/time.h>
#include <store.hpp>
#include <thread>
#include <codec/bpv6.h>
#include <util/tsc.h>
#include <reg.hpp>
#include <ingress.h>
#include <egress.h>

#include <fcntl.h>
#include <unistd.h>


// Used for forking python process
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>    /* for fork, exec, kill */
#include <sys/types.h> /* for pid_t            */
#include <sys/wait.h>  /* for waitpid          */
#include <signal.h>    /* for SIGTERM, SIGKILL */

// Prototypes
std::string GetEnv( const std::string & var );

volatile bool RUN_BPGEN = true;
volatile bool RUN_INGRESS = true;
volatile bool RUN_EGRESS = true;
volatile bool RUN_STORAGE = true;

pid_t spawnPythonServer(void) {


    int outfd = open("1.txt", O_CREAT|O_WRONLY|O_TRUNC, 0644);
    if (!outfd)
    {
        perror("open");
        return EXIT_FAILURE;
    }


    // fork a child process, execute vlc, and return it's pid. Returns -1 if fork failed.
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }
    // when you call fork(), it creates two copies of your program: a parent, and a child. You can tell them apart by the return
    // value from fork().  If fork() returns 0, this is is the child process.  If fork() returns non-zero, we are the parent and the
    // return value is the PID of the child process. */
    if (pid == 0) {
        // this is the child process.  now we can call one of the exec family of functions to execute VLC.  When you call exec,
        // it replaces the currently running process (the child process) with whatever we pass to exec.  So our child process will now
        // be running VLC.  exec() will never return except in an error case, since it is now running the VLC code and not our code.
        std::string commandArg = GetEnv("HDTN_SOURCE_ROOT") + "/common/regsvr/main.py";
        std::cout << "Running python3 " << commandArg << std::endl << std::flush;
        execlp("python3", commandArg.c_str(), (char*)NULL);
        std::cerr << "ERROR Running python3 " << commandArg << std::endl << std::flush;
        perror("python3");
        abort();

    } else {
        // parent, return the child's PID
        return pid;
    }
}

int killProcess(pid_t processId) {
    // kill will send the specified signal to the specified process. Send a TERM signal to VLC, requesting that it terminate.
    // If that doesn't work, we send a KILL signal.  If that doesn't work, we give up.
    if (kill(processId, SIGTERM) < 0) {
        perror("kill with SIGTERM");
        if (kill(processId, SIGKILL) < 0) {
            perror("kill with SIGKILL");
        }
    }
    // This shows how we can get the exit status of our child process.  It will wait for the the VLC process to exit, then grab it's return value
    int status = 0;
    waitpid(processId, &status, 0);
    return status;
}

int main_test_vlc()
{
    pid_t vlc = spawnPythonServer();

    if (vlc == -1) {
        fprintf(stderr, "failed to fork child process\n");
        return 0;
    }

    printf("spawned vlc with pid %d\n", vlc);

    sleep(3);

    /* kill will send the specified signal to the specified process.
     * in this case, we send a TERM signal to VLC, requesting that it
     * terminate.  If that doesn't work, we send a KILL signal.
     * If that doesn't work, we give up. */
    if (kill(vlc, SIGTERM) < 0) {
        perror("kill with SIGTERM");
        if (kill(vlc, SIGKILL) < 0) {
            perror("kill with SIGKILL");
        }
    }

    /* this shows how we can get the exit status of our child process.
     * it will wait for the the VLC process to exit, then grab it's return
     * value. */
    int status = 0;
    waitpid(vlc, &status, 0);
    printf("VLC exited with status %d\n", WEXITSTATUS(status));

    return 0;
}


int runBpgen() {
    std::cout << "Start runBpgen ... " << std::endl << std::flush;
    struct bpgen_hdr {
        uint64_t seq;
        uint64_t tsc;
        timespec abstime;
    };
    int BP_MSG_BUFSZ  = 65536;
    int BP_MSG_NBUF = 32;
    struct mmsghdr* msgbuf;
    uint64_t bundle_count = 0;
    uint64_t bundle_data = 0;
    uint64_t raw_data = 0;
    uint64_t rate = 50;
    std::string target  =  "127.0.0.1";
    struct sockaddr_in servaddr;
    int fd = socket(AF_INET,SOCK_DGRAM,0);
    int source_node = 1;
    int dest_node = 1;
    int port = 4556;
    size_t gen_sz = 1500;
    ssize_t res;
//    char* logfile = (char*)malloc(2048);
//    snprintf(logfile, 2048,"bpgen.%lu.csv", time(0));
//    snprintf(logfile, 2048,"bpgen.test1.csv");
//    FILE* log = NULL;
//    log = fopen(logfile, "w+");
//    if(NULL == log) {
//        perror("fopen()");
//        return -5;
//    }
    printf("Generating bundles of size %d\n", (int)gen_sz);
    if(rate) {
        printf("Generating up to %d bundles / second.\n", (int)rate);
    }
    printf("Bundles will be destinated for %s:%d\n", target.c_str(), port);
    fflush(stdout);
    char* data_buffer[gen_sz];
    memset(data_buffer, 0, gen_sz);
    bpgen_hdr* hdr = (bpgen_hdr*)data_buffer;
    uint64_t last_time = 0;
    uint64_t curr_time = 0;
    uint64_t seq = 0;
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(target.c_str());
    servaddr.sin_port = htons(port);
    msgbuf = (mmsghdr*) malloc(sizeof(struct mmsghdr) * BP_MSG_NBUF);
    memset(msgbuf, 0, sizeof(struct mmsghdr) * BP_MSG_NBUF);
    struct iovec* io = (iovec*) malloc(sizeof(struct iovec) * BP_MSG_NBUF);
    memset(io, 0, sizeof(struct iovec) * BP_MSG_NBUF);
    char** tmp = (char**) malloc(BP_MSG_NBUF * sizeof(char *));
    int i = 0;
    for(i = 0; i < BP_MSG_NBUF; ++i) {
        tmp[i] = (char*) malloc(BP_MSG_BUFSZ);
        memset(tmp[i], 0x0, BP_MSG_BUFSZ);
    }
    for(i = 0; i < BP_MSG_NBUF; ++i) {
        msgbuf[i].msg_hdr.msg_iov = &io[i];
        msgbuf[i].msg_hdr.msg_iovlen = 1;
        msgbuf[i].msg_hdr.msg_iov->iov_base = tmp[i];
        msgbuf[i].msg_hdr.msg_iov->iov_len = BP_MSG_BUFSZ;
        msgbuf[i].msg_hdr.msg_name = (void *)&servaddr;
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
    if(rate) {
        sval = 1000000.0 / rate;   // sleep val in usec
        sval *= BP_MSG_NBUF;
        printf("Sleeping for %f usec between bursts\n", sval);
        fflush(stdout);
    }
    uint64_t bseq = 0;
    uint64_t totalBundleCount = 0;
    uint64_t totalSize = 0;
    while(RUN_BPGEN) {
        for(int idx = 0; idx < BP_MSG_NBUF; ++idx) {
            char* curr_buf = (char*)(msgbuf[idx].msg_hdr.msg_iov->iov_base);
            curr_time = time(0);
            if(curr_time == last_time) {
                ++seq;
            }
            else {
                gettimeofday(&tv, NULL);
                double elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
                elapsed -= start;
                start = start + elapsed;
//                fprintf(log, "%0.6f, %lu, %lu, %lu, %lu\n", elapsed, bundle_count, raw_data, bundle_data, tsc_total);
//                fflush(log);
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
            primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) | bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT);
            primary.src_node = source_node;
            primary.src_svc = 1;
            primary.dst_node = dest_node;
            primary.dst_svc = 1;
            primary.creation = (uint64_t)bpv6_unix_to_5050(curr_time);
            primary.sequence = seq;
            uint64_t tsc_start = rdtsc();
            bundle_length = bpv6_primary_block_encode(&primary, curr_buf, 0, BP_MSG_BUFSZ);
            tsc_total += rdtsc() - tsc_start;
            block.type = BPV6_BLOCKTYPE_PAYLOAD;
            block.flags = BPV6_BLOCKFLAG_LAST_BLOCK;
            block.length = gen_sz;
            tsc_start = rdtsc();
            bundle_length += bpv6_canonical_block_encode(&block, curr_buf, bundle_length, BP_MSG_BUFSZ);
            tsc_total += rdtsc() - tsc_start;
            hdr->tsc = rdtsc();
            clock_gettime(CLOCK_REALTIME, &hdr->abstime);
            hdr->seq = bseq++;
            memcpy(curr_buf + bundle_length, data_buffer, gen_sz);
            bundle_length += gen_sz;
            msgbuf[idx].msg_hdr.msg_iov[0].iov_len = bundle_length;
            ++bundle_count;
            bundle_data += gen_sz;     // payload data
            raw_data += bundle_length; // bundle overhead + payload data
        } // End for(idx = 0; idx < BP_MSG_NBUF; ++idx) {



        res = sendmmsg(fd, msgbuf, BP_MSG_NBUF, 0);
        if(res < 0) {
            perror("cannot send message");
        } else {
            totalSize += msgbuf->msg_len;
            totalBundleCount += bundle_count;
            std::cout << "In BPGEN, totalBundleCount: " << totalBundleCount << " , totalSize: " << totalSize << std::endl << std::flush;
        }
        if(rate) {
            usleep((uint64_t)sval);
        }
    }
    close(fd);
    std::cout << "End runBpgen ... " << std::endl << std::flush;
    return 0;
}

int runIngress() {
    std::cout << "Start runIngress ... " << std::endl << std::flush;
    int INGRESS_PORT = 4556;
    hdtn::bp_ingress ingress;
    ingress.init(BP_INGRESS_TYPE_UDP);
    uint64_t last_time = 0;
    uint64_t curr_time = 0;
    //finish registration stuff -ingress will find out what egress services have registered
    hdtn::hdtn_regsvr regsvr;
    regsvr.init("tcp://127.0.0.1:10140", "ingress", 10149, "PUSH");
    regsvr.reg();
    hdtn::hdtn_entries res = regsvr.query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
    }
    printf("Announcing presence of ingress engine ...\n");
    fflush(stdout);
    ingress.netstart(INGRESS_PORT);
    int count = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    fflush(stdout);
    while (RUN_INGRESS) {
        curr_time = time(0);
        gettimeofday(&tv, NULL);
        ingress.elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        ingress.elapsed -= start;
//        std::cout << "Before ingress.update" << std::endl << std::flush;
        //count = ingress.update();
        count = ingress.update(0.5); // Use timeout so call does not indefinitely block.  Units are seconds
//        std::cout << "After ingress.update, count = " << count << std::endl << std::flush;
        if (count > 0) {
            ingress.process(count);
        }
        last_time = curr_time;
    }
    std::cout << "End runIngress ... " << std::endl << std::flush;
    std::cout << "In runIngress, bundle_count: " << ingress.bundle_count << " , ingress.bundle_data: " << ingress.bundle_data << std::endl << std::flush;

    // Write summary to file
//    std::ofstream output;
//    //std::string current_date = hdtn::datetime();
//    output.open("ingress-test1.txt");
//    output << "Elapsed, Bundle Count (M),Rate (Mbps),Bundles/sec, Bundle Data (MB)\n";
//    double rate = 8 * ((ingress.bundle_data / (double)(1024 * 1024)) / ingress.elapsed);
//    output << ingress.elapsed << "," << ingress.bundle_count / 1000000.0f << "," << rate << "," << ingress.bundle_count / ingress.elapsed << ", " << ingress.bundle_data / (double)(1024 * 1024) << "\n";
//    output.close();

    return 0;
}

int runEgress() {


//    char* logfile = (char*)malloc(2048);
//    snprintf(logfile, 2048,"egress.%lu.csv", time(0));
//    FILE* log = NULL;
//    log = fopen(logfile, "w+");
//    if(NULL == log) {
//        perror("fopen()");
//        return -5;
//    }


    std::cout << "Start runEgress ... " << std::endl << std::flush;
    uint64_t bundle_count = 0;
    uint64_t bundle_data = 0;
    uint64_t message_count = 0;
    double elapsed = 0;
    hdtn::hegr_manager egress;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start Egress: +%f\n", start);
    fflush(stdout);
    //finish registration stuff - egress should register, ingress will query
    hdtn::hdtn_regsvr regsvr;
    regsvr.init("tcp://127.0.0.1:10140", "egress", 10149, "PULL");
    regsvr.reg();
    hdtn::hdtn_entries res = regsvr.query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl << std::flush;
    }
    zmq::context_t *zmq_ctx;
    zmq::socket_t *zmq_sock;
    zmq_ctx = new zmq::context_t;
    zmq_sock = new zmq::socket_t(*zmq_ctx, zmq::socket_type::pull);
    zmq_sock->connect("tcp://127.0.0.1:10149");
    egress.init();
    int entry_status;
    entry_status = egress.add(1, HEGR_FLAG_UDP, "127.0.0.1", 4557);
    if (!entry_status) {
        return 0;  //error message prints in add function
    }
    printf("Announcing presence of egress ...\n");
    fflush(stdout);
    for (int i = 0; i < 8; ++i) {
        egress.up(i);
    }
    int bundle_size = 0;

    // JCF, set timeout
    // Use a form of receive that times out so we can terminate cleanly.
    int timeout = 250; // milliseconds
    zmq_sock->setsockopt(ZMQ_RCVTIMEO,&timeout,sizeof(int));

    while (RUN_EGRESS) {
        gettimeofday(&tv, NULL);
        elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        elapsed -= start;
        zmq::message_t hdr;
        zmq::message_t message;

        // JCF
        // Use a form of receive that times out so we can terminate cleanly.  If no message was received after timeout go back to top of loop
        //std::cout << "In runEgress, before recv. " << std::endl << std::flush;
        bool retValue = zmq_sock->recv(&hdr);
        //std::cout << "In runEgress, after recv. retValue = " << retValue << " , hdr.size() = " << hdr.size() << std::endl << std::flush;
        if (!retValue) {
            continue;
        }

        message_count++;
        char bundle[HMSG_MSG_MAX];
        if (hdr.size() < sizeof(hdtn::common_hdr)) {
            std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl << std::flush;
            return -1;
        }



        hdtn::common_hdr *common = (hdtn::common_hdr *)hdr.data();


//        std::cout << "In runEgress, after recv. common->type = " << common->type << std::endl << std::flush;
//        switch (common->type) {
//            case HDTN_MSGTYPE_STORE:
//                zmq_sock->recv(&message);
//                bundle_size = message.size();
//                memcpy(bundle, message.data(), bundle_size);
//                egress.forward(1, bundle, bundle_size);
//                bundle_data += bundle_size;
//                bundle_count++;
//                break;
//        }


        zmq_sock->recv(&message);
        bundle_size = message.size();
        memcpy(bundle, message.data(), bundle_size);
        bundle_data += bundle_size;
        bundle_count++;



    }



    // Write summary to file
//    std::ofstream output;
//    output.open("egress-test1.txt");
//    output << "Elapsed, Bundle Count (M), Rate (Mbps),Bundles/sec,Message Count (M)\n";
//    double rate = 8 * ((bundle_data / (double)(1024 * 1024)) / elapsed);
//    output << elapsed << ", " << bundle_count / 1000000.0f << ", " << rate << ", " << bundle_count / elapsed << "," << message_count / 1000000.0f << "\n";
//    output.close();
    // Clean up resources
    zmq_sock->close();
    delete zmq_sock;
    delete zmq_ctx;
    std::cout << "End runEgress ... " << std::endl << std::flush;
    std::cout << "In runEgress, bundle_count: " << bundle_count << " , bundle_data: " << bundle_data << std::endl << std::flush;

    return 0;
}

int runStorage() {
    std::cout << "Start runStorage ... " << std::endl << std::flush;
    uint64_t last_bytes = 0;
    uint64_t last_count = 0;
    double last = 0.0;
    timeval tv;
    gettimeofday(&tv, NULL);
    last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    hdtn::storage_config config;
    config.regsvr = "tcp://127.0.0.1:10140";
    config.local = "tcp://127.0.0.1:10145";
    config.store_path = "/tmp/hdtn.store";
    hdtn::storage store;
    std::cout << "[store] Initializing storage ..." << std::endl;
    if (!store.init(config)) {
        return -1;
    }
    while (RUN_STORAGE) {
        store.update();
        gettimeofday(&tv, NULL);
        double curr = (tv.tv_sec + (tv.tv_usec / 1000000.0));
        if (curr - last > 1) {
            last = curr;
            hdtn::storage_stats *stats = store.stats();
            uint64_t cbytes = stats->in_bytes - last_bytes;
            uint64_t ccount = stats->in_msg - last_count;
            last_bytes = stats->in_bytes;
            last_count = stats->in_msg;
            printf("[store] Received: %ld msg / %0.2f MB\n", ccount, cbytes / (1024.0 * 1024.0));
        }
    }
    std::cout << "End runStorage ... " << std::endl << std::flush;
    return 0;
}

std::string GetEnv( const std::string & var ) {
     const char * val = std::getenv( var.c_str() );
     if ( val == nullptr ) { // invalid to assign nullptr to std::string
         return "";
     }
     else {
         return val;
     }
}

void startRegistrationService() {
    // Start the registration service
    std::string hdtnSourceRoot = GetEnv("HDTN_SOURCE_ROOT");
    std::string commandStartReg = "python3 " + hdtnSourceRoot + "/common/regsvr/main.py &";
    system(commandStartReg.c_str());
    std::cout << " >>>>> Started the registration service." << std::endl;
}

void stopRegistrationService() {
    // Stop the registration service
    std::cout << "Stopping the registration service." << std::endl;
    std::string target = "tcp://127.0.0.1:10140";
    std::string svc = "test";
    uint16_t port = 10140;
    std::string mode = "PUSH";
    zmq::context_t * zmq_ctx = new zmq::context_t;
    zmq::socket_t * zmq_sock = new zmq::socket_t(*zmq_ctx, zmq::socket_type::req);
    char tbuf[255];
    memset(tbuf, 0, 255);
    snprintf(tbuf, 255, "%s:%d:%s", svc.c_str(), port, mode.c_str());
    zmq_sock->setsockopt(ZMQ_IDENTITY, (void *)tbuf, target.size());
    zmq_sock->connect(target);
    zmq_sock->send("SHUTDOWN", strlen("SHUTDOWN"), 0);
    zmq_sock->close();
    delete zmq_sock;
    delete zmq_ctx;
    std::cout << " <<<<< Stopped the registration service." << std::endl;
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


    //sleep(1);
    std::thread threadIngress(runIngress);
//    sleep(1);
    std::thread threadEgress(runEgress);

    sleep(1);
    std::thread threadBpgen(runBpgen);

    sleep(3);

    RUN_BPGEN = false;
    threadBpgen.join();

    sleep(2);

    RUN_INGRESS = false;
    threadIngress.join();

    sleep(1);
    RUN_EGRESS = false;
    std::cout << "Before threadEgress.join(). " << std::endl << std::flush;
    threadEgress.join();
    std::cout << "After threadEgress.join(). " << std::endl << std::flush;

//    killProcess(pidPythonServer);

    std::cout << "End Integrated Tests. " << std::endl << std::flush;

    sleep(3);
    return true;
}




//int main(int ac, char*av[]) {

//	std::cout << "Running Integrated Tests. " << std::endl << std::flush;

////    pid_t pidPythonServer = spawnPythonServer();
////    if (pidPythonServer == -1) {
////        fprintf(stderr, "failed to fork child process\n");
////        return -1;
////    }
////    printf("Spawned python server with pid %d\n", pidPythonServer);
////    fflush(stdout);


//    //sleep(1);
//    std::thread threadIngress(runIngress);
////    sleep(1);
//    std::thread threadEgress(runEgress);

//    sleep(1);
//    std::thread threadBpgen(runBpgen);

//    sleep(3);

//    RUN_BPGEN = false;
//    threadBpgen.join();

//    sleep(2);

//    RUN_INGRESS = false;
//    threadIngress.join();

//    sleep(1);
//    RUN_EGRESS = false;
//    std::cout << "Before threadEgress.join(). " << std::endl << std::flush;
//    threadEgress.join();
//    std::cout << "After threadEgress.join(). " << std::endl << std::flush;

////    killProcess(pidPythonServer);

//    std::cout << "End Integrated Tests. " << std::endl << std::flush;

//    sleep(3);
//	return 0;
//}
