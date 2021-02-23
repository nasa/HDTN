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
//#include "message.hpp"
#ifndef _WIN32
#include "util/tsc.h"
#endif
#include "BpSinkAsync.h"
#include <boost/bind.hpp>

#define BP_MSG_BUFSZ             (65536)
#define BP_BUNDLE_DEFAULT_SZ     (100)
#define BP_GEN_BUNDLE_MAXSZ      (64000)
#define BP_GEN_RATE_MAX          (1 << 30)
#define BP_GEN_TARGET_DEFAULT    "127.0.0.1"
#define BP_GEN_PORT_DEFAULT      (4556)
#define BP_GEN_SRC_NODE_DEFAULT  (1)
#define BP_GEN_DST_NODE_DEFAULT  (2)
#define BP_GEN_BATCH_DEFAULT     (1 << 18)  // write out one entry per this many bundles.
#define BP_GEN_LOGFILE           "bpsink.%lu.csv"

#define BP_MSG_NBUF   (32)


namespace hdtn {

struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};

BpSinkAsync::BpSinkAsync(uint16_t port, bool useTcp, const std::string & thisLocalEidString) :
    m_rxPortUdpOrTcp(port),
    m_useTcp(useTcp),
    M_THIS_EID_STRING(thisLocalEidString),
    m_udpSocket(m_ioService),
    m_tcpAcceptor(m_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
    m_circularIndexBuffer(BP_MSG_NBUF),
    m_udpReceiveBuffersCbVec(BP_MSG_NBUF),
    m_remoteEndpointsCbVec(BP_MSG_NBUF),
    m_udpReceiveBytesTransferredCbVec(BP_MSG_NBUF),
    m_running(false)
{

}

BpSinkAsync::~BpSinkAsync() {
    //stop the socket before stopping the io service
    if (m_udpSocket.is_open()) {
        try {
            m_udpSocket.close();
        } catch (const boost::system::system_error & e) {
            std::cerr << "Error closing UDP socket in BpSinkAsync::~BpSinkAsync():  " << e.what() << std::endl;
        }
    }
    m_tcpAcceptor.close();
    m_tcpclBundleSinkPtr = boost::shared_ptr<TcpclBundleSink>();
    m_ioService.stop(); //stop may not be needed

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

int BpSinkAsync::Init(uint32_t type) {
    m_batch = BP_GEN_BATCH_DEFAULT;


    m_tscTotal = 0;
    m_rtTotal = 0;
    m_totalBytesRx = 0;

    m_receivedCount = 0;
    m_duplicateCount = 0;
    m_seqHval = 0;
    m_seqBase = 0;
    if(!m_useTcp) { //udp only stuff
        for (unsigned int i = 0; i < BP_MSG_NBUF; ++i) {
            m_udpReceiveBuffersCbVec[i].resize(BP_MSG_BUFSZ);
        }
        if (!m_running) {
            m_running = true;
            m_threadCbReaderPtr = boost::make_shared<boost::thread>(
                boost::bind(&BpSinkAsync::PopCbThreadFunc, this)); //create and start the worker thread


        }
    }
    return 0;
}


int BpSinkAsync::Netstart() {
    if(m_ioServiceThreadPtr) {
        std::cerr << "Error in BpSinkAsync::Netstart: already running" << std::endl;
        return 1;
    }
    printf("Starting BpSinkAsync channel ...\n");
    if(m_useTcp) {
        StartTcpAccept();
    }
    else {
        //Receiver UDP
        try {
            m_udpSocket.open(boost::asio::ip::udp::v4());
            m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), m_rxPortUdpOrTcp));
        } catch (const boost::system::system_error & e) {
            std::cerr << "Could not bind on UDP port " << m_rxPortUdpOrTcp << std::endl;
            std::cerr <<  "  Error: " << e.what() << std::endl;

            m_running = false;
            return 0;
        }
        printf("BpSinkAsync bound successfully on UDP port %d ...", m_rxPortUdpOrTcp);
        StartUdpReceive(); //call before creating io_service thread so that it has "work"
    }
    m_ioServiceThreadPtr = boost::make_shared<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

    return 0;
}

int BpSinkAsync::Process(const std::vector<uint8_t> & rxBuf, const std::size_t messageSize) {

    {
        bpv6_primary_block primary;
        bpv6_canonical_block payload;
        const char * const mbuf = (const char*)rxBuf.data();
        uint64_t curr_seq = 0;

        int32_t offset = 0;
        offset += bpv6_primary_block_decode(&primary, mbuf, offset, messageSize);
        if(0 == offset) {
            printf("Malformed bundle received - aborting.\n");
            return -2;
        }
        bool is_payload = false;
        while(!is_payload) {
            int32_t tres = bpv6_canonical_block_decode(&payload, mbuf, offset, messageSize);
            if(tres <= 0) {
                printf("Failed to parse extension block - aborting.\n");
                return -3;
            }
            offset += tres;
            if(payload.type == BPV6_BLOCKTYPE_PAYLOAD) {
                is_payload = true;
            }
        }
        m_totalBytesRx += payload.length;
        bpgen_hdr* data = (bpgen_hdr*)(mbuf + offset);
        // offset by the first sequence number we see, so that we don't need to restart for each run ...
        if(m_seqBase == 0) {
            m_seqBase = data->seq;
            m_seqHval = m_seqBase;
            ++m_receivedCount; //brian added
        }
        else if(data->seq > m_seqHval) {
            m_seqHval = data->seq;
            ++m_receivedCount;
        }
        else {
            ++m_duplicateCount;
        }
#ifndef _WIN32
        timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        int64_t one_way = 1000000 * (((int64_t)tp.tv_sec) - ((int64_t)data->abstime.tv_sec));
        one_way += (((int64_t)tp.tv_nsec) - ((int64_t)data->abstime.tv_nsec)) / 1000;

        m_rtTotal += one_way;
        m_tscTotal += rdtsc() - data->tsc;
#endif // !_WIN32
    }
    //gettimeofday(&tv, NULL);
    //double curr_time = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    //curr_time -= start;
/*
    if(m_duplicateCount + m_receivedCount >= m_batch) {
        if(m_receivedCount == 0) {
            printf("BUG: batch was entirely duplicates - this shouldn't actually be possible.\n");
        }
        else if(use_one_way) {
            fprintf(log, "%0.6f, %lu, %lu, %lu, %lu, %lu, %lu, %0.4f%%, %0.4f, one_way\n",
                    curr_time, seq_base, seq_hval, received_count, duplicate_count,
                    bytes_total, rt_total,
                    100 - (100 * (received_count / (double)(seq_hval - seq_base))),
                    1000 * ((rt_total / 1000000.0) / received_count) );

        }
        else {
            fprintf(log, "%0.6f, %lu, %lu, %lu, %lu, %lu, %lu, %0.4f%%, %0.4f, rtt\n",
                    curr_time, seq_base, seq_hval, received_count, duplicate_count,
                    bytes_total, tsc_total,
                    100 - (100 * (received_count / (double)(seq_hval - seq_base))),
                    1000 * ((tsc_total / (double)freq_base) / received_count) );

        }
        fflush(log);

        duplicate_count = 0;
        received_count = 0;
        bytes_total = 0;
        seq_hval = 0;
        seq_base = 0;
        rt_total = 0;
        tsc_total = 0;
    }
    */
    return 0;
}


void BpSinkAsync::StartUdpReceive() {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == UINT32_MAX) {
        std::cerr << "critical error in BpSinkAsync::StartUdpReceive(): buffers full.. UDP receiving on BpSinkAsync will now stop!\n";
        return;
    }

    m_udpSocket.async_receive_from(
                boost::asio::buffer(m_udpReceiveBuffersCbVec[writeIndex]),
                m_remoteEndpointsCbVec[writeIndex],
                boost::bind(&BpSinkAsync::HandleUdpReceive, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred,
                            writeIndex));
}

void BpSinkAsync::HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    //std::cout << "1" << std::endl;
    if (!error) {
        m_udpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_conditionVariableCb.notify_one();
        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "critical error in BpSinkAsync::HandleUdpReceive(): " << error.message() << std::endl;
    }
}

void BpSinkAsync::TcpclWholeBundleReadyCallback(boost::shared_ptr<std::vector<uint8_t> > wholeBundleSharedPtr) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    Process(*wholeBundleSharedPtr, wholeBundleSharedPtr->size());
}

void BpSinkAsync::StartTcpAccept() {
    std::cout << "waiting for tcp connections\n";
    boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_tcpAcceptor.get_executor()); //get_io_service() is deprecated: Use get_executor()

    m_tcpAcceptor.async_accept(*newTcpSocketPtr,
        boost::bind(&BpSinkAsync::HandleTcpAccept, this, newTcpSocketPtr,
            boost::asio::placeholders::error));
}

void BpSinkAsync::HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr, const boost::system::error_code& error) {
    if (!error) {
        std::cout << "tcp connection: " << newTcpSocketPtr->remote_endpoint().address() << ":" << newTcpSocketPtr->remote_endpoint().port() << "\n";
        if((m_tcpclBundleSinkPtr) && !m_tcpclBundleSinkPtr->ReadyToBeDeleted() ) {
            std::cout << "warning: bpsink received a new tcp connection, but there is an old connection that is active.. old connection will be stopped" << std::endl;
        }
        m_tcpclBundleSinkPtr = boost::make_shared<TcpclBundleSink>(newTcpSocketPtr,
                                                                   boost::bind(&BpSinkAsync::TcpclWholeBundleReadyCallback, this, boost::placeholders::_1),
                                                                   50, 2000, M_THIS_EID_STRING);

        StartTcpAccept(); //only accept if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cout << "tcp accept error: " << error.message() << "\n";
    }
}

void BpSinkAsync::PopCbThreadFunc() {

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

        m_circularIndexBuffer.CommitRead();
    }

    std::cout << "BpSinkAsync Circular buffer reader thread exiting\n";

}



}  // namespace hdtn
