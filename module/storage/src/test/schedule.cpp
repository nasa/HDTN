#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <cstdlib>
#include <iostream>

#include "message.hpp"
#include "paths.hpp"
#include "reg.hpp"


//This test code is used to send storage release messages
//to enable development of the contact schedule and bundle
//storage release mechanism.


int main(int argc, char *argv[]) {
    hdtn::HdtnRegsvr reg;
    reg.Init(HDTN_REG_SERVER_PATH, "scheduler", 10200, "pub");
    reg.Reg();
    zmq::context_t ctx;
    zmq::socket_t socket(ctx, zmq::socket_type::pub);
    socket.bind(HDTN_BOUND_SCHEDULER_PUBSUB_PATH);
    reg.Init(HDTN_REG_SERVER_PATH, "ingress", 10110, "push");
    reg.Reg();
    zmq::socket_t storesocket(ctx, zmq::socket_type::push);
    storesocket.bind(HDTN_BOUND_INGRESS_TO_CONNECTING_STORAGE_PATH);

    boost::this_thread::sleep(boost::posix_time::seconds(10));

    const int bufferSize = 1000;
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
    
    uint64_t total_bytes = 0;
    uint64_t total_msg = 0;
    for (int i = 0; i < 100000; i++) {        
        hdtn::BlockHdr block;
        memset(&block, 0, sizeof(hdtn::BlockHdr));
        block.base.type = HDTN_MSGTYPE_STORE;
        block.flowId = 1;
        block.bundleSeq = i;
        storesocket.send(&block, sizeof(hdtn::BlockHdr), 0/*ZMQ_MORE*/);
        storesocket.send(data, bufferSize, 0);
        j++;
        total_msg++;
        total_bytes = total_bytes + bufferSize;
    }

    
    std::cout << "Bytes sent: " << total_bytes << ", messages sent:" << total_msg << std::endl;
    //std::cout << "Mbps: " << (double)(total_bytes * 8) / (1024 * 1024) / (curr - start) << ", messages per sec " << (double)total_msg / (curr - start) << std::endl;

    std::cout << "sleep 30 before sending release message\n";
    hdtn::IreleaseStartHdr releaseMsg;
    hdtn::IreleaseStopHdr stopMsg;
    boost::this_thread::sleep(boost::posix_time::seconds(30));

    memset(&releaseMsg, 0, sizeof(hdtn::IreleaseStartHdr));
    releaseMsg.base.type = HDTN_MSGTYPE_IRELSTART;
    releaseMsg.flowId = 1;
    releaseMsg.rate = 0;  //not implemented
    releaseMsg.duration = 20;//not implemented
    socket.send(&releaseMsg, sizeof(hdtn::IreleaseStartHdr), 0);
    std::cout << "Release message sent \n";
 
   

    return 0;
}
