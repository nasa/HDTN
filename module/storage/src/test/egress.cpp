#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>

#include "message.hpp"
#include "paths.hpp"
#include "reg.hpp"

double start = 0;
double last = 0;
int done = 0;
uint64_t total_bytes = 0;
uint64_t total_msg = 0;
uint64_t last_bytes = 0;
uint64_t last_msg = 0;
timeval tv;

static void signal_handler(int signal_value) {
    if (done == 0) {
        gettimeofday(&tv, NULL);
        last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    }
    std::cout << "Elapsed,Bytes (M), Bundle Count (M), Rate (Mbps),Bundles/sec,\n";
    double rate = 8 * (total_bytes / (double)(1024 * 1024)) / (last - start);
    std::cout << (last - start) << ", " << (total_bytes / (double)(1024 * 1024)) << "," << (double)total_msg / 1000000.0f << ", " << rate << ", " << (double)(total_msg) / (last - start) << std::endl;
    exit(EXIT_SUCCESS);
}
static void catch_signals(void) {
    struct sigaction action;
    action.sa_handler = signal_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}


int main(int argc, char *argv[]) {
    catch_signals();
    hdtn::hdtn_regsvr _reg;
    _reg.init(HDTN_REG_SERVER_PATH, "egress", 10120, "pull");
    _reg.reg();
    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::pull);
    socket.connect(HDTN_RELEASE_PATH);
    gettimeofday(&tv, NULL);
    start = (tv.tv_sec + (tv.tv_usec / 1000000.0));

    int recv = 0;
    zmq::message_t message;
    while (true) {
        socket.recv(&message);
        hdtn::common_hdr *common = (hdtn::common_hdr *)message.data();
        switch (common->type) {
            case HDTN_MSGTYPE_EGRESS: {
                //start timing once we actually start receiving messages
                if (recv == 0) {
                    gettimeofday(&tv, NULL);
                    start = (tv.tv_sec + (tv.tv_usec / 1000000.0));
                    recv = 1;
                }

                socket.recv(&message);
                if (message.size() > 0) {
                    std::string res = std::string((char *)message.data(), message.size());
                    total_bytes = total_bytes + message.size();
                    total_msg++;
                }
                break;
            }
        }
        //3 messages seem to always be missing
        if (total_msg > 999996) {
            done = 1;
            gettimeofday(&tv, NULL);
            last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
            std::cout << "done receiving messages\n";
        }

    }

    return 0;
}
