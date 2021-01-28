#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdlib>
#include <iostream>

#include "message.hpp"
#include "paths.hpp"
#include "reg.hpp"

//This test code is used to send storage release messages
//to enable development of the contact schedule and bundle
//storage release mechanism.

int main(int argc, char *argv[]) {
    hdtn::hdtn_regsvr _reg;
    _reg.init(HDTN_REG_SERVER_PATH, "scheduler", 10200, "pub");
    _reg.reg();
    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::pub);
    socket.bind(HDTN_SCHEDULER_PATH);
    _reg.init(HDTN_REG_SERVER_PATH, "ingress", 10110, "push");
    _reg.reg();
    zmq::socket_t storesocket(ctx, zmq::socket_type::push);
    storesocket.bind(HDTN_STORAGE_PATH);
    boost::asio::io_service io;
    boost::asio::deadline_timer timer(io, boost::posix_time::seconds(10));
    timer.wait();

    int bufferSize = 1000;
    char data[bufferSize];
    char alpha = 'A';
    int j = 1;
    for (int i = 0; i < bufferSize; i++) {
        data[i] = alpha;
        alpha++;
        if (alpha == '[') {
            alpha = 'A';
        }
    }
    timeval tv;
    gettimeofday(&tv, NULL);
    double start = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    double curr = 0;
    uint64_t total_bytes = 0;
    uint64_t total_msg = 0;
    for (int i = 0; i < 100000; i++) {
        char ihdr[sizeof(hdtn::block_hdr)];
        hdtn::block_hdr *block = (hdtn::block_hdr *)ihdr;
        memset(ihdr, 0, sizeof(hdtn::block_hdr));
        block->base.type = HDTN_MSGTYPE_STORE;
        block->flow_id = 1;
        block->bundle_seq = i;
        storesocket.send(ihdr, sizeof(hdtn::block_hdr), ZMQ_MORE);
        storesocket.send(data, bufferSize, 0);
        j++;
        total_msg++;
        total_bytes = total_bytes + bufferSize;
    }
    gettimeofday(&tv, NULL);
    curr = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    std::cout << "Bytes sent: " << total_bytes << ", messages sent:" << total_msg << std::endl;
    std::cout << "Mbps: " << (double)(total_bytes * 8) / (1024 * 1024) / (curr - start) << ", messages per sec " << (double)total_msg / (curr - start) << std::endl;

    std::cout << "sleep 30 before sending release message\n";
    boost::asio::io_service io2;
    char relHdr[sizeof(hdtn::irelease_start_hdr)];
    hdtn::irelease_start_hdr *releaseMsg = (hdtn::irelease_start_hdr *)relHdr;
    char stopHdr[sizeof(hdtn::irelease_stop_hdr)];
    hdtn::irelease_stop_hdr *stopMsg = (hdtn::irelease_stop_hdr *)stopHdr;
    boost::asio::deadline_timer timer2(io2, boost::posix_time::seconds(30));
    timer2.wait();

    memset(relHdr, 0, sizeof(hdtn::irelease_start_hdr));
    releaseMsg->base.type = HDTN_MSGTYPE_IRELSTART;
    releaseMsg->flow_id = 1;
    releaseMsg->rate = 0;  //not implemented
    releaseMsg->duration = 20;//not implemented
    socket.send(releaseMsg, sizeof(hdtn::irelease_start_hdr), 0);
    std::cout << "Release message sent \n";
 
   

    return 0;
}
