/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

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
#include <boost/make_unique.hpp>

#include <boost/make_unique.hpp>

namespace hdtn {

BpIngressSyscall::BpIngressSyscall() :
    m_eventsTooManyInStorageQueue(0),
    m_eventsTooManyInEgressQueue(0),
    m_running(false)
{
}

BpIngressSyscall::~BpIngressSyscall() {
    Stop();
}

void BpIngressSyscall::Stop() {
    if (m_tcpAcceptorPtr) {
        if (m_tcpAcceptorPtr->is_open()) {
            try {
                m_tcpAcceptorPtr->close();
            }
            catch (const boost::system::system_error & e) {
                std::cerr << "Error closing TCP Acceptor in BpIngressSyscall::Stop():  " << e.what() << std::endl;
            }
        }        
        m_tcpAcceptorPtr.reset(); //delete it
    }
    m_listTcpclBundleSinkPtrs.clear();
    m_listStcpBundleSinkPtrs.clear();
    m_udpBundleSinkPtr.reset();
    if (!m_ioService.stopped()) {
        m_ioService.stop(); //ioservice doesn't need stopped at this point but just in case
    }

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }


    m_running = false; //thread stopping criteria

    if (m_threadZmqAckReaderPtr) {
        m_threadZmqAckReaderPtr->join();
        m_threadZmqAckReaderPtr.reset(); //delete it
    }

    
    
    std::cout << "m_eventsTooManyInStorageQueue: " << m_eventsTooManyInStorageQueue << std::endl;
}

int BpIngressSyscall::Init(uint32_t type) {
    
    if (!m_running) {
        m_running = true;

        // socket for cut-through mode straight to egress
        m_zmqCtx_ingressEgressPtr = boost::make_unique<zmq::context_t>();
        m_zmqPushSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::push);
        m_zmqPushSock_boundIngressToConnectingEgressPtr->bind(HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH);
        // socket for sending bundles to storage
        m_zmqCtx_ingressStoragePtr = boost::make_unique<zmq::context_t>();
        m_zmqPushSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressStoragePtr, zmq::socket_type::push);
        m_zmqPushSock_boundIngressToConnectingStoragePtr->bind(HDTN_BOUND_INGRESS_TO_CONNECTING_STORAGE_PATH);
        // socket for receiving acks from storage
        m_zmqPullSock_connectingStorageToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressStoragePtr, zmq::socket_type::pull);
        m_zmqPullSock_connectingStorageToBoundIngressPtr->bind(HDTN_CONNECTING_STORAGE_TO_BOUND_INGRESS_PATH);
        // socket for receiving acks from egress
        m_zmqPullSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::pull);
        m_zmqPullSock_connectingEgressToBoundIngressPtr->bind(HDTN_CONNECTING_EGRESS_TO_BOUND_INGRESS_PATH);
        static const int timeout = 250;  // milliseconds
        m_zmqPullSock_connectingStorageToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);
        m_zmqPullSock_connectingEgressToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);

        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&BpIngressSyscall::ReadZmqAcksThreadFunc, this)); //create and start the worker thread
    }
    return 0;
}


int BpIngressSyscall::Netstart(uint16_t port, bool useTcpcl, bool useStcp, bool useLtp, bool alwaysSendToStorage,
    uint64_t thisLtpEngineId, uint64_t ltpReportSegmentMtu, uint64_t oneWayLightTimeMs,
    uint64_t oneWayMarginTimeMs, uint64_t clientServiceId, uint64_t estimatedFileSizeToReceive,
    unsigned int numLtpUdpRxPacketsCircularBufferSize, unsigned int maxLtpRxUdpPacketSizeBytes)
{
    m_useTcpcl = useTcpcl;
    m_useStcp = useStcp;
    m_alwaysSendToStorage = alwaysSendToStorage;
    if(m_ioServiceThreadPtr) {
        std::cerr << "Error in BpIngressSyscall::Netstart: already running" << std::endl;
        return 1;
    }
    printf("Starting ingress channel ...\n");
    if (useLtp) {
        m_ltpBundleSinkPtr = boost::make_unique<LtpBundleSink>(
            boost::bind(&BpIngressSyscall::WholeBundleReadyCallback, this, boost::placeholders::_1),
            thisLtpEngineId, 1, ltpReportSegmentMtu,
            boost::posix_time::milliseconds(oneWayLightTimeMs), boost::posix_time::milliseconds(oneWayMarginTimeMs),
            port, numLtpUdpRxPacketsCircularBufferSize,
            maxLtpRxUdpPacketSizeBytes, estimatedFileSizeToReceive);
    }
    else {
        //Receiver UDP
        m_udpBundleSinkPtr = boost::make_unique<UdpBundleSink>(m_ioService, port,
            boost::bind(&BpIngressSyscall::WholeBundleReadyCallback, this, boost::placeholders::_1),
            200, 65536);
    }
    m_tcpAcceptorPtr = boost::make_unique<boost::asio::ip::tcp::acceptor>(m_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) ;
    StartTcpAccept();
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

    return 0;
}

void BpIngressSyscall::ReadZmqAcksThreadFunc() {

    static const unsigned int NUM_SOCKETS = 2;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_connectingEgressToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;
    std::size_t totalAcksFromStorage = 0;

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    BlockHdr rblk;
    while (m_running) { //keep thread alive if running
        if (zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL) > 0) {
            if (items[0].revents & ZMQ_POLLIN) { //ack from egress
                const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingEgressToBoundIngressPtr->recv(zmq::mutable_buffer(&rblk, sizeof(hdtn::BlockHdr)), zmq::recv_flags::dontwait);
                if (!res) {
                    std::cerr << "error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read egress BlockHdr ack" << std::endl;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::BlockHdr))) {
                    std::cerr << "egress blockhdr message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::BlockHdr) << std::endl;
                }
                else {
                    m_egressAckMapQueueMutex.lock();
                    EgressToIngressAckingQueue & egressToIngressAckingObj = m_egressAckMapQueue[rblk.flowId];
                    m_egressAckMapQueueMutex.unlock();
                    if (egressToIngressAckingObj.CompareAndPop_ThreadSafe(rblk)) {
                        egressToIngressAckingObj.NotifyAll();
                        ++totalAcksFromEgress;
                    }
                    else {
                        std::cerr << "error didn't receive expected egress ack" << std::endl;
                    }
                    
                }
            }
            if (items[1].revents & ZMQ_POLLIN) { //ack from storage
                const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingStorageToBoundIngressPtr->recv(zmq::mutable_buffer(&rblk, sizeof(hdtn::BlockHdr)), zmq::recv_flags::dontwait);
                if (!res) {
                    std::cerr << "error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read storage BlockHdr ack" << std::endl;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::BlockHdr))) {
                    std::cerr << "egress blockhdr message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::BlockHdr) << std::endl;
                }
                else {
                    bool needsNotify = false;
                    {
                        boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
                        if (m_storageAckQueue.empty()) {
                            std::cerr << "error m_storageAckQueue is empty" << std::endl;
                        }
                        else if (*m_storageAckQueue.front() == rblk) {
                            m_storageAckQueue.pop();
                            needsNotify = true;
                            ++totalAcksFromStorage;
                        }
                        else {
                            std::cerr << "error didn't receive expected storage ack" << std::endl;
                        }
                    }
                    if (needsNotify) {
                        m_conditionVariableStorageAckReceived.notify_all();
                    }
                }
            }
        }
    }
    std::cout << "totalAcksFromEgress: " << totalAcksFromEgress << std::endl;
    std::cout << "totalAcksFromStorage: " << totalAcksFromStorage << std::endl;
    std::cout << "m_bundleCountStorage: " << m_bundleCountStorage << std::endl;
    std::cout << "m_bundleCountEgress: " << m_bundleCountEgress << std::endl;
    m_bundleCount = m_bundleCountStorage + m_bundleCountEgress;
    std::cout << "m_bundleCount: " << m_bundleCount << std::endl;
    std::cout << "BpIngressSyscall::ReadZmqAcksThreadFunc thread exiting\n";
}

static void CustomCleanup(void *data, void *hint) {
    //std::cout << "free " << static_cast<std::vector<uint8_t>*>(hint)->size() << std::endl;
    delete static_cast<std::vector<uint8_t>*>(hint);
}

static void CustomIgnoreCleanupBlockHdr(void *data, void *hint) {}

int BpIngressSyscall::Process(std::vector<uint8_t> && rxBuf) {  //TODO: make buffer zmq message to reduce copy
    const std::size_t messageSize = rxBuf.size();
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
        std::unique_ptr<BlockHdr> hdrPtr = boost::make_unique<BlockHdr>();
        hdtn::BlockHdr & hdr = *hdrPtr;
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
                m_egressAckMapQueueMutex.lock();
                EgressToIngressAckingQueue & egressToIngressAckingObj = m_egressAckMapQueue[hdr.flowId];
                m_egressAckMapQueueMutex.unlock();
                for (unsigned int attempt = 0; attempt < 8; ++attempt) { //2000 ms timeout
                    if (egressToIngressAckingObj.GetQueueSize() > 5) {
                        if (attempt == 7) {
                            std::cerr << "error: too many pending egress acks in the queue for flow id " << hdr.flowId << std::endl;
                        }
                        egressToIngressAckingObj.WaitUntilNotifiedOr250MsTimeout();
                        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                        ++m_eventsTooManyInEgressQueue;
                        continue; //next attempt
                    }
                    {
                        zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below
                        boost::mutex::scoped_lock lock(m_ingressToEgressZmqSocketMutex);
                        if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(messageWithDataStolen, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                            std::cerr << "ingress can't send BlockHdr to egress" << std::endl;
                        }
                        else {
                            egressToIngressAckingObj.PushMove_ThreadSafe(hdrPtr);
                            if (numChunks == 1) {
                                //this is an optimization because we only have one chunk to send
                                //The zmq_msg_init_data() function shall initialise the message object referenced by msg
                                //to represent the content referenced by the buffer located at address data, size bytes long.
                                //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
                                //If provided, the deallocation function ffn shall be called once the data buffer is no longer
                                //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
                                std::vector<uint8_t> * rxBufRawPointer = new std::vector<uint8_t>(std::move(rxBuf));
                                zmq::message_t messageWithDataStolen(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanup, rxBufRawPointer);
                                if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
                                    std::cerr << "ingress can't send bundle to egress" << std::endl;
                                }
                                else {
                                    //success                            
                                    ++m_bundleCountEgress;
                                }
                            }
                            else {
                                if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(zmq::const_buffer(&tbuf[CHUNK_SIZE * j], bytesToSend), zmq::send_flags::dontwait)) {
                                    std::cerr << "ingress can't send bundle to egress" << std::endl;
                                }
                                else {
                                    //success                            
                                    ++m_bundleCountEgress;
                                }
                            }
                        }
                    }
                    break;

                }
            }
            else { //storage
                boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
                for(unsigned int attempt = 0; attempt < 8; ++attempt) { //2000 ms timeout
                    if (m_storageAckQueue.size() > 5) {
                        if (attempt == 7) {
                            std::cerr << "error: too many pending storage acks in the queue" << std::endl;
                        }
                        m_conditionVariableStorageAckReceived.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
                        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                        ++m_eventsTooManyInStorageQueue;
                        continue; //next attempt
                    }

                    zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below

                    //zmq threads not thread safe but protected by mutex above
                    if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(messageWithDataStolen, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                        std::cerr << "ingress can't send BlockHdr to storage" << std::endl;
                    }
                    else {
                        m_storageAckQueue.push(std::move(hdrPtr));
                        if (numChunks == 1) {
                            //this is an optimization because we only have one chunk to send
                            //The zmq_msg_init_data() function shall initialise the message object referenced by msg
                            //to represent the content referenced by the buffer located at address data, size bytes long.
                            //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
                            //If provided, the deallocation function ffn shall be called once the data buffer is no longer
                            //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
                            std::vector<uint8_t> * rxBufRawPointer = new std::vector<uint8_t>(std::move(rxBuf));
                            zmq::message_t messageWithDataStolen(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanup, rxBufRawPointer);
                            if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
                                std::cerr << "ingress can't send bundle to storage" << std::endl;
                            }
                            else {
                                //success                            
                                ++m_bundleCountStorage;
                            }
                        }
                        else {
                            if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(zmq::const_buffer(&tbuf[CHUNK_SIZE * j], bytesToSend), zmq::send_flags::dontwait)) {
                                std::cerr << "ingress can't send bundle to storage" << std::endl;
                            }
                            else {
                                //success                            
                                ++m_bundleCountStorage;
                            }
                        }
                    }
                    break;
                    
                }
            }
            
            ++m_zmsgsOut;
        }
        
        m_bundleData += messageSize;
        ////timer = rdtsc() - timer;
    }
    return 0;
}



void BpIngressSyscall::WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    Process(std::move(wholeBundleVec));
}

void BpIngressSyscall::StartTcpAccept() {
    std::cout << "waiting for tcp connections\n";
    boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_ioService); //get_io_service() is deprecated: Use get_executor()

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
            std::unique_ptr<TcpclBundleSink> bundleSinkPtr = boost::make_unique<TcpclBundleSink>(newTcpSocketPtr, m_ioService,
                                                                                                   boost::bind(&BpIngressSyscall::WholeBundleReadyCallback, this, boost::placeholders::_1),
                                                                                                   200, 20000, "ingress");
            m_listTcpclBundleSinkPtrs.push_back(std::move(bundleSinkPtr));
        }
        else if (m_useStcp) {
            std::unique_ptr<StcpBundleSink> bundleSinkPtr = boost::make_unique<StcpBundleSink>(newTcpSocketPtr,
                                                                                                   boost::bind(&BpIngressSyscall::WholeBundleReadyCallback, this, boost::placeholders::_1),
                                                                                                   200);
            m_listStcpBundleSinkPtrs.push_back(std::move(bundleSinkPtr));
        }
        

        StartTcpAccept(); //only accept if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cout << "tcp accept error: " << error.message() << "\n";
    }


}

void BpIngressSyscall::RemoveInactiveTcpConnections() {
    m_listTcpclBundleSinkPtrs.remove_if([](const std::unique_ptr<TcpclBundleSink> & ptr){ return ptr->ReadyToBeDeleted();});
    m_listStcpBundleSinkPtrs.remove_if([](const std::unique_ptr<StcpBundleSink> & ptr) { return ptr->ReadyToBeDeleted(); });
}

}  // namespace hdtn
