#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <cassert>
#include <iostream>

#include "codec/bpv6-ext-block.h"
#include "codec/bpv6.h"
#include "ingress.h"
#include "message.hpp"
#include <boost/bind.hpp>

namespace hdtn {

BpIngressSyscall::BpIngressSyscall() :
    m_udpSocket(m_ioService),
    m_circularIndexBuffer(BP_INGRESS_MSG_NBUF),
    m_udpReceiveBuffersCbVec(BP_INGRESS_MSG_NBUF),
    m_remoteEndpointsCbVec(BP_INGRESS_MSG_NBUF),
    m_udpReceiveBytesTransferredCbVec(BP_INGRESS_MSG_NBUF),
    m_running(false)
{
}

BpIngressSyscall::~BpIngressSyscall() {
    //stop the socket before stopping the io service
    if (m_udpSocket.is_open()) {
        try {
            m_udpSocket.close();
        } catch (const boost::system::system_error & e) {
            std::cerr << "Error closing UDP socket in BpIngressSyscall::~BpIngressSyscall():  " << e.what() << std::endl;
        }
    }
    //m_ioService.stop(); //ioservice should not require stopping as socket.close will result in it running out of work

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr = boost::shared_ptr<boost::thread>();
    }


    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr = boost::shared_ptr<boost::thread>();
    }
}

int BpIngressSyscall::Init(uint32_t type) {
    for (unsigned int i = 0; i < BP_INGRESS_MSG_NBUF; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(BP_INGRESS_MSG_BUFSZ);
    }
    if (!m_running) {
        m_running = true;
        m_threadCbReaderPtr = boost::make_shared<boost::thread>(
            boost::bind(&BpIngressSyscall::PopCbThreadFunc, this)); //create and start the worker thread

        // socket for cut-through mode straight to egress
        m_zmqCutThroughCtx = boost::make_shared<zmq::context_t>();
        m_zmqCutThroughSock = boost::make_shared<zmq::socket_t>(*m_zmqCutThroughCtx, zmq::socket_type::push);
        m_zmqCutThroughSock->bind(m_cutThroughAddress);
        // socket for sending bundles to storage
        m_zmqStorageCtx = boost::make_shared<zmq::context_t>();
        m_zmqStorageSock = boost::make_shared<zmq::socket_t>(*m_zmqStorageCtx, zmq::socket_type::push);
        m_zmqStorageSock->bind(m_storageAddress);
    }
    return 0;
}


int BpIngressSyscall::Netstart(uint16_t port) {
    if(m_ioServiceThreadPtr) {
        std::cerr << "Error in BpIngressSyscall::Netstart: already running" << std::endl;
        return 1;
    }
    printf("Starting ingress channel ...\n");
    //Receiver UDP
    try {
        m_udpSocket.open(boost::asio::ip::udp::v4());
        m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port));
    } catch (const boost::system::system_error & e) {
        std::cerr << "Could not create port " << std::to_string(port) << std::endl;
        std::cerr <<  "  Error: " << e.what() << std::endl;

        m_running = false;
        return 0;
    }
    printf("Ingress bound successfully on port %d ...", port);
    StartUdpReceive(); //call before creating io_service thread so that it has "work"
    m_ioServiceThreadPtr = boost::make_shared<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

    return 0;
}

int BpIngressSyscall::Process(const std::vector<uint8_t> & rxBuf, const std::size_t messageSize) {
    uint64_t timer;    
    uint64_t offset = 0;
    uint32_t zframeSeq = 0;
    bpv6_primary_block bpv6Primary;
    bpv6_eid dst;
    char hdrBuf[sizeof(BlockHdr)];
    memset(&bpv6Primary, 0, sizeof(bpv6_primary_block));
    {
        const char * const tbuf = (const char*)rxBuf.data(); //char tbuf[HMSG_MSG_MAX];
        hdtn::BlockHdr hdr;
        memset(&hdr, 0, sizeof(hdtn::BlockHdr));
        timer = rdtsc();
        //memcpy(tbuf, m_bufs[i], recvlen);
        hdr.ts = timer;
        offset = bpv6_primary_block_decode(&bpv6Primary, (const char*)rxBuf.data(), offset, messageSize);
        dst.node = bpv6Primary.dst_node;
        hdr.flowId = dst.node;  // for now
        hdr.base.flags = bpv6Primary.flags;
        hdr.base.type = HDTN_MSGTYPE_STORE;
        // hdr.ts=recvlen;
        int numChunks = 1;
        std::size_t bytesToSend = messageSize;
        int remainder = 0;

        zframeSeq = 0;
        if (messageSize > CHUNK_SIZE)  // the bundle is bigger than internal message size limit
        {
            numChunks = messageSize / CHUNK_SIZE;
            bytesToSend = CHUNK_SIZE;
            remainder = messageSize % CHUNK_SIZE;
            if (remainder != 0) numChunks++;
        }
        for (int j = 0; j < numChunks; j++) {
            if ((j == numChunks - 1) && (remainder != 0)) bytesToSend = remainder;
            hdr.bundleSeq = m_ingSequenceNum;
            m_ingSequenceNum++;
            hdr.zframe = zframeSeq;
            zframeSeq++;
            memcpy(hdrBuf, &hdr, sizeof(BlockHdr));
            m_zmqCutThroughSock->send(hdrBuf, sizeof(BlockHdr), ZMQ_MORE);
            //char data[bytesToSend];
            //memcpy(data, tbuf + (CHUNK_SIZE * j), bytesToSend);
            //m_zmqCutThroughSock->send(data, bytesToSend, 0);
            m_zmqCutThroughSock->send(&tbuf[CHUNK_SIZE * j], bytesToSend, 0);
            ++m_zmsgsOut;
        }
        ++m_bundleCount;
        m_bundleData += messageSize;
        timer = rdtsc() - timer;
    }
    return 0;
}


void BpIngressSyscall::StartUdpReceive() {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == UINT32_MAX) {
        std::cerr << "critical error in BpIngressSyscall::StartUdpReceive(): buffers full.. UDP receiving on ingress will now stop!\n";
        return;
    }
            //m_udpReceiveBuffersCbVec[i].resize(BP_INGRESS_MSG_BUFSZ);
    m_udpSocket.async_receive_from(
                boost::asio::buffer(m_udpReceiveBuffersCbVec[writeIndex]),
                m_remoteEndpointsCbVec[writeIndex],
                boost::bind(&BpIngressSyscall::HandleUdpReceive, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred,
                            writeIndex));
}

void BpIngressSyscall::HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    //std::cout << "1" << std::endl;
    if (!error) {
        m_udpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_conditionVariableCb.notify_one();
        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "critical error in BpIngressSyscall::HandleUdpReceive(): " << error.message() << std::endl;
    }
}

void BpIngressSyscall::PopCbThreadFunc() {

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (m_circularIndexBuffer.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile

        if (consumeIndex == UINT32_MAX) { //if empty
            m_conditionVariableCb.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        Process(m_udpReceiveBuffersCbVec[consumeIndex], m_udpReceiveBytesTransferredCbVec[consumeIndex]);
        //std::cout << "2" << std::endl;
        m_circularIndexBuffer.CommitRead();
    }

    std::cout << "Ingress Circular buffer reader thread exiting\n";

}

}  // namespace hdtn
