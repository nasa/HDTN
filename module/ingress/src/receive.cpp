#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    m_eventsTooManyInStorageQueue(0),
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
    m_tcpAcceptorPtr->close();
    m_tcpAcceptorPtr = boost::shared_ptr<boost::asio::ip::tcp::acceptor>();
    m_listTcpclBundleSinkPtrs.clear();
    m_listStcpBundleSinkPtrs.clear();
    m_ioService.stop(); //ioservice doesn't need stopped at this point but just in case

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr = boost::shared_ptr<boost::thread>();
    }


    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr = boost::shared_ptr<boost::thread>();
    }

    std::cout << "m_eventsTooManyInStorageQueue: " << m_eventsTooManyInStorageQueue << std::endl;
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
        m_zmqCtx_boundIngressToConnectingEgressPtr = boost::make_shared<zmq::context_t>();
        m_zmqPushSock_boundIngressToConnectingEgressPtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_boundIngressToConnectingEgressPtr, zmq::socket_type::push);
        m_zmqPushSock_boundIngressToConnectingEgressPtr->bind(HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH);
        // socket for sending bundles to storage
        m_zmqCtx_boundIngressToConnectingStoragePtr = boost::make_shared<zmq::context_t>();
        m_zmqPushSock_boundIngressToConnectingStoragePtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_boundIngressToConnectingStoragePtr, zmq::socket_type::push);
        m_zmqPushSock_boundIngressToConnectingStoragePtr->bind(HDTN_BOUND_INGRESS_TO_CONNECTING_STORAGE_PATH);
        // socket for receiving acks from storage
        m_zmqPullSock_connectingStorageToboundIngressPtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_boundIngressToConnectingStoragePtr, zmq::socket_type::pull);
        m_zmqPullSock_connectingStorageToboundIngressPtr->bind(HDTN_CONNECTING_STORAGE_TO_BOUND_INGRESS_PATH);
        static const int timeout = 1;  // milliseconds
        m_zmqPullSock_connectingStorageToboundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);
    }
    return 0;
}


int BpIngressSyscall::Netstart(uint16_t port, bool useTcpcl, bool useStcp, bool alwaysSendToStorage) {
    m_useTcpcl = useTcpcl;
    m_useStcp = useStcp;
    m_alwaysSendToStorage = alwaysSendToStorage;
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
    m_tcpAcceptorPtr = boost::make_shared<boost::asio::ip::tcp::acceptor>(m_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) ;
    StartTcpAccept();
    m_ioServiceThreadPtr = boost::make_shared<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

    return 0;
}

int BpIngressSyscall::Process(const std::vector<uint8_t> & rxBuf, const std::size_t messageSize) {  //TODO: make buffer zmq message to reduce copy
	//uint64_t timer = 0;
    uint64_t offset = 0;
    uint32_t zframeSeq = 0;
    bpv6_primary_block bpv6Primary;
    bpv6_eid dst;
    memset(&bpv6Primary, 0, sizeof(bpv6_primary_block));
    {
        const char * const tbuf = (const char*)rxBuf.data(); //char tbuf[HMSG_MSG_MAX];
        
        ////timer = rdtsc();
        //memcpy(tbuf, m_bufs[i], recvlen);
        //hdr.ts = timer;
        offset = bpv6_primary_block_decode(&bpv6Primary, (const char*)rxBuf.data(), offset, messageSize);
        const uint64_t absExpirationUsec = (bpv6Primary.creation * 1000000) + bpv6Primary.sequence + bpv6Primary.lifetime;
        const uint32_t priority = bpv6_bundle_get_priority(bpv6Primary.flags);
        //std::cout << "p: " << priority << " lt " << bpv6Primary.lifetime << " c " << bpv6Primary.creation << ":" << bpv6Primary.sequence << " a: " << absExpirationUsec <<  std::endl;
        dst.node = bpv6Primary.dst_node;

        
        
        //m_storageAckQueueMutex.lock();
        //m_storageAckQueue.
        //
        //m_storageAckQueueMutex.unlock();
        std::unique_ptr<BlockHdr> hdrUptr = std::make_unique<BlockHdr>();
        hdtn::BlockHdr & hdr = *hdrUptr;
        memset(&hdr, 0, sizeof(hdtn::BlockHdr));
        hdr.flowId = static_cast<uint32_t>(dst.node);  // for now
        hdr.base.flags = static_cast<uint16_t>(bpv6Primary.flags);
        hdr.base.type = (m_alwaysSendToStorage) ? HDTN_MSGTYPE_STORE : HDTN_MSGTYPE_EGRESS;
        hdr.ts = absExpirationUsec; //use ts for now
        hdr.ttl = priority; //use ttl for now
        // hdr.ts=recvlen;
        std::size_t numChunks = 1;
        std::size_t bytesToSend = messageSize;
        std::size_t remainder = 0;

        zframeSeq = 0;
        if (messageSize > CHUNK_SIZE)  // the bundle is bigger than internal message size limit
        {
            numChunks = messageSize / CHUNK_SIZE;
            bytesToSend = CHUNK_SIZE;
            remainder = messageSize % CHUNK_SIZE;
            if (remainder != 0) numChunks++;
        }
        for (std::size_t j = 0; j < numChunks; ++j) {
            if ((j == numChunks - 1) && (remainder != 0)) bytesToSend = remainder;
            hdr.bundleSeq = m_ingSequenceNum;  //TODO make thread safe
            m_ingSequenceNum++;
            hdr.zframe = zframeSeq;
            zframeSeq++;
            if (hdr.base.type == HDTN_MSGTYPE_EGRESS) {
                m_zmqPushSock_boundIngressToConnectingEgressPtr->send(zmq::const_buffer(&hdr, sizeof(BlockHdr)), zmq::send_flags::sndmore);                
                m_zmqPushSock_boundIngressToConnectingEgressPtr->send(zmq::const_buffer(&tbuf[CHUNK_SIZE * j], bytesToSend), zmq::send_flags::none);
            }
            else { //storage

                //first use this thread to do the work of emptying any storage acks
                zmq::message_t rhdr;
                boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
                zmq::recv_flags rflags = zmq::recv_flags::dontwait; //first attempt don't wait
                for(unsigned int attempt = 0; attempt < 2000; ++attempt) { //2000 ms timeout
                    while (m_zmqPullSock_connectingStorageToboundIngressPtr->recv(rhdr, rflags)) { //socket already has 1ms timeout
                        if (rhdr.size() == sizeof(BlockHdr)) {
                            BlockHdr rblk;
                            memcpy(&rblk, rhdr.data(), sizeof(BlockHdr));
                            //std::cout << "ack: " << m_storageAckQueue.size() << std::endl;
                            if (m_storageAckQueue.empty()) {
                                std::cerr << "error m_storageAckQueue is empty" << std::endl;
                            }
                            else if (!m_storageAckQueue.front()) {
                                std::cerr << "error q element is null" << std::endl;
                            }
                            else if (*m_storageAckQueue.front() == rblk) {
                                m_storageAckQueue.pop();
                            }
                            else {
                                std::cerr << "error didn't receive expected ack" << std::endl;
                            }
                            //std::cout << "done ack: " << m_storageAckQueue.size() << std::endl;
                        }
                    }
                    if (m_storageAckQueue.size() > 5) {
                        rflags = zmq::recv_flags::none; //second and subsequent attempts wait up to 1 ms
                        ++m_eventsTooManyInStorageQueue;
                    }

                    if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(zmq::const_buffer(&hdr, sizeof(BlockHdr)), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                        std::cerr << "ingress can't send BlockHdr to storage" << std::endl;
                    }
                    else {                        
                        if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(zmq::const_buffer(&tbuf[CHUNK_SIZE * j], bytesToSend), zmq::send_flags::dontwait)) {
                            std::cerr << "ingress can't send bundle to storage" << std::endl;
                        }
                        else {
                            //success
                            m_storageAckQueue.push(std::move(hdrUptr));
                        }
                    }
                    break;
                    
                }
            }
            
            ++m_zmsgsOut;
        }
        ++m_bundleCount;  //todo thread safe
        m_bundleData += messageSize;
        ////timer = rdtsc() - timer;
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


void BpIngressSyscall::TcpclWholeBundleReadyCallback(boost::shared_ptr<std::vector<uint8_t> > wholeBundleSharedPtr) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    Process(*wholeBundleSharedPtr, wholeBundleSharedPtr->size());
}

void BpIngressSyscall::StartTcpAccept() {
    std::cout << "waiting for tcp connections\n";
    boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_tcpAcceptorPtr->get_executor()); //get_io_service() is deprecated: Use get_executor()

    m_tcpAcceptorPtr->async_accept(*newTcpSocketPtr,
        boost::bind(&BpIngressSyscall::HandleTcpAccept, this, newTcpSocketPtr,
            boost::asio::placeholders::error));
}

void BpIngressSyscall::HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr, const boost::system::error_code& error) {
    if (!error) {
        std::cout << "tcp connection: " << newTcpSocketPtr->remote_endpoint().address() << ":" << newTcpSocketPtr->remote_endpoint().port() << "\n";
        //boost::shared_ptr<TcpclBundleSink> bundleSinkPtr = boost::make_shared<TcpclBundleSink>(newTcpSocketPtr,)
        //std::list<boost::shared_ptr<TcpclBundleSink> > m_listTcpclBundleSinkPtrs;
        //if((m_tcpclBundleSinkPtr) && !m_tcpclBundleSinkPtr->ReadyToBeDeleted() ) {
        //    std::cout << "warning: bpsink received a new tcp connection, but there is an old connection that is active.. old connection will be stopped" << std::endl;
        //}
        if (m_useTcpcl) {
            boost::shared_ptr<TcpclBundleSink> bundleSinkPtr = boost::make_shared<TcpclBundleSink>(newTcpSocketPtr,
                                                                                                   boost::bind(&BpIngressSyscall::TcpclWholeBundleReadyCallback, this, boost::placeholders::_1),
                                                                                                   50, 2000, "ingress");
            m_listTcpclBundleSinkPtrs.push_back(bundleSinkPtr);
        }
        else if (m_useStcp) {
            boost::shared_ptr<StcpBundleSink> bundleSinkPtr = boost::make_shared<StcpBundleSink>(newTcpSocketPtr,
                                                                                                   boost::bind(&BpIngressSyscall::TcpclWholeBundleReadyCallback, this, boost::placeholders::_1),
                                                                                                   50);
            m_listStcpBundleSinkPtrs.push_back(bundleSinkPtr);
        }
        

        StartTcpAccept(); //only accept if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cout << "tcp accept error: " << error.message() << "\n";
    }


}

void BpIngressSyscall::RemoveInactiveTcpConnections() {
    m_listTcpclBundleSinkPtrs.remove_if([](const boost::shared_ptr<TcpclBundleSink> & ptr){ return ptr->ReadyToBeDeleted();});
    m_listStcpBundleSinkPtrs.remove_if([](const boost::shared_ptr<StcpBundleSink> & ptr) { return ptr->ReadyToBeDeleted(); });
}

}  // namespace hdtn
