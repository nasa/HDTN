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
//#include "message.hpp"
#include "util/tsc.h"
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

BpSinkAsync::BpSinkAsync(uint16_t port, bool useTcp) :
    m_rxPortUdpOrTcp(port),
    m_useTcp(useTcp),
    m_udpSocket(m_ioService),
    m_tcpAcceptor(m_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
    m_noKeepAlivePacketReceivedTimer(m_ioService),
    m_needToSendKeepAliveMessageTimer(m_ioService),
    m_circularIndexBuffer(BP_MSG_NBUF),
    m_udpReceiveBuffersCbVec(BP_MSG_NBUF),
    m_remoteEndpointsCbVec(BP_MSG_NBUF),
    m_udpReceiveBytesTransferredCbVec(BP_MSG_NBUF),
    m_running(false)
{
    if(useTcp) {
        m_tcpcl.SetContactHeaderReadCallback(boost::bind(&BpSinkAsync::ContactHeaderCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
        m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&BpSinkAsync::DataSegmentCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
        m_tcpcl.SetAckSegmentReadCallback(boost::bind(&BpSinkAsync::AckCallback, this, boost::placeholders::_1));
        m_tcpcl.SetBundleRefusalCallback(boost::bind(&BpSinkAsync::BundleRefusalCallback, this, boost::placeholders::_1));
        m_tcpcl.SetNextBundleLengthCallback(boost::bind(&BpSinkAsync::NextBundleLengthCallback, this, boost::placeholders::_1));
        m_tcpcl.SetKeepAliveCallback(boost::bind(&BpSinkAsync::KeepAliveCallback, this));
        m_tcpcl.SetShutdownMessageCallback(boost::bind(&BpSinkAsync::ShutdownCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));
    }
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
    ShutdownAndCloseTcpSocket();
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
    for (unsigned int i = 0; i < BP_MSG_NBUF; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(BP_MSG_BUFSZ);
    }
    if (!m_running) {
        m_running = true;
        m_threadCbReaderPtr = boost::make_shared<boost::thread>(
            boost::bind(&BpSinkAsync::PopCbThreadFunc, this)); //create and start the worker thread


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
        }
        else if(data->seq > m_seqHval) {
            m_seqHval = data->seq;
            ++m_receivedCount;
        }
        else {
            ++m_duplicateCount;
        }
        timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        int64_t one_way = 1000000 * (((int64_t)tp.tv_sec) - ((int64_t)data->abstime.tv_sec));
        one_way += (((int64_t)tp.tv_nsec) - ((int64_t)data->abstime.tv_nsec)) / 1000;

        m_rtTotal += one_way;
        m_tscTotal += rdtsc() - data->tsc;
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

void BpSinkAsync::StartTcpAccept() {
    std::cout << "waiting for tcp connections\n";
    boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_tcpAcceptor.get_executor()); //get_io_service() is deprecated: Use get_executor()

    m_tcpAcceptor.async_accept(*newTcpSocketPtr,
        boost::bind(&BpSinkAsync::HandleTcpAccept, this, newTcpSocketPtr,
            boost::asio::placeholders::error));
}

void BpSinkAsync::HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr, const boost::system::error_code& error) {
    if (!error) {
        ShutdownAndCloseTcpSocket(); //also reset tcpcl states
        std::cout << "tcp connection: " << newTcpSocketPtr->remote_endpoint().address() << ":" << newTcpSocketPtr->remote_endpoint().port() << "\n";
        m_tcpSocketPtr = newTcpSocketPtr;
        StartTcpReceive();
    }
    else {
        std::cout << "tcp accept error: " << error.message() << "\n";
    }

    StartTcpAccept();
}

void BpSinkAsync::StartTcpReceive() {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == UINT32_MAX) {
        std::cerr << "critical error in BpSinkAsync::StartTcpReceive(): buffers full.. UDP receiving on BpSinkAsync will now stop!\n";
        return;
    }

    m_tcpSocketPtr->async_read_some(
        boost::asio::buffer(m_udpReceiveBuffersCbVec[writeIndex]),
        boost::bind(&BpSinkAsync::HandleTcpReceiveSome, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred,
            writeIndex));
}
void BpSinkAsync::HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    if (!error) {
        m_udpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_conditionVariableCb.notify_one();
        StartTcpReceive(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "Tcp connection closed cleanly by peer" << std::endl;
        ShutdownAndCloseTcpSocket();
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "Error in BpSinkAsync::HandleTcpReceiveSome: " << error.message() << std::endl;
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
        if(m_useTcp) {
            m_tcpcl.HandleReceivedChars(m_udpReceiveBuffersCbVec[consumeIndex].data(), m_udpReceiveBytesTransferredCbVec[consumeIndex]);
        }
        else {
            Process(m_udpReceiveBuffersCbVec[consumeIndex], m_udpReceiveBytesTransferredCbVec[consumeIndex]);
        }
        //std::cout << "2" << std::endl;
        m_circularIndexBuffer.CommitRead();
    }

    std::cout << "BpSinkAsync Circular buffer reader thread exiting\n";

}

void BpSinkAsync::HandleTcpSend(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred, bool closeSocket) {
    if(error) {
        std::cerr << "error in BpSinkAsync::HandleTcpSend: " << error.message() << std::endl;
        ShutdownAndCloseTcpSocket();
    }
    else if(closeSocket) {
        ShutdownAndCloseTcpSocket();
    }
}

void BpSinkAsync::ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid) {
    m_contactHeaderFlags = flags;
    //The keepalive_interval parameter is set to the minimum value from
    //both contact headers.  If one or both contact headers contains the
    //value zero, then the keepalive feature (described in Section 5.6)
    //is disabled.
    m_keepAliveIntervalSeconds = keepAliveIntervalSeconds;
    m_localEid = localEid;
    std::cout << "received contact header from " << m_localEid << std::endl;

    //Since BpSink was waiting for a contact header, it just got one.  Now it's time to reply with a contact header
    //use the same keepalive interval
    if(m_tcpSocketPtr) {
        boost::shared_ptr<std::vector<uint8_t> > contactHeaderPtr = boost::make_shared<std::vector<uint8_t> >();
        Tcpcl::GenerateContactHeader(*contactHeaderPtr, static_cast<CONTACT_HEADER_FLAGS>(0), keepAliveIntervalSeconds, "bpsink");
        boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*contactHeaderPtr),
                                         boost::bind(&BpSinkAsync::HandleTcpSend, this, contactHeaderPtr,
                                                     boost::asio::placeholders::error,
                                                     boost::asio::placeholders::bytes_transferred, false));
        if(m_keepAliveIntervalSeconds) { //non-zero
            std::cout << "using " << keepAliveIntervalSeconds << " seconds for keepalive\n";

            // * 2 =>
            //If no message (KEEPALIVE or other) has been received for at least
            //twice the keepalive_interval, then either party MAY terminate the
            //session by transmitting a one-byte SHUTDOWN message (as described in
            //Table 2) and by closing the TCP connection.
            m_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds * 2));
            m_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&BpSinkAsync::OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));


            m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds));
            m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&BpSinkAsync::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
        }
    }
}

void BpSinkAsync::DataSegmentCallback(boost::shared_ptr<std::vector<uint8_t> > dataSegmentDataSharedPtr, bool isStartFlag, bool isEndFlag) {

    uint32_t bytesToAck = 0;
    if(isStartFlag && isEndFlag) { //optimization for whole (non-fragmented) data
        Process(*dataSegmentDataSharedPtr, dataSegmentDataSharedPtr->size());
        bytesToAck = static_cast<uint32_t>(dataSegmentDataSharedPtr->size());
    }
    else {
        if (isStartFlag) {
            m_fragmentedBundleRxConcat.resize(0);
        }
        m_fragmentedBundleRxConcat.insert(m_fragmentedBundleRxConcat.end(), dataSegmentDataSharedPtr->begin(), dataSegmentDataSharedPtr->end()); //concatenate
        bytesToAck = static_cast<uint32_t>(m_fragmentedBundleRxConcat.size());
        if(isEndFlag) { //fragmentation complete
            Process(m_fragmentedBundleRxConcat, m_fragmentedBundleRxConcat.size());
        }
    }
    //send ack
    if((static_cast<unsigned int>(CONTACT_HEADER_FLAGS::REQUEST_ACK_OF_BUNDLE_SEGMENTS)) & (static_cast<unsigned int>(m_contactHeaderFlags))) {
        if(m_tcpSocketPtr) {
            boost::shared_ptr<std::vector<uint8_t> > ackPtr = boost::make_shared<std::vector<uint8_t> >();
            Tcpcl::GenerateAckSegment(*ackPtr, bytesToAck);
            boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*ackPtr),
                                             boost::bind(&BpSinkAsync::HandleTcpSend, this, ackPtr,
                                                         boost::asio::placeholders::error,
                                                         boost::asio::placeholders::bytes_transferred, false));
        }
    }
}

void BpSinkAsync::AckCallback(uint32_t totalBytesAcknowledged) {
    std::cout << "bpsink should never enter AckCallback" << std::endl;
}

void BpSinkAsync::BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode) {
    std::cout << "bpsink should never enter BundleRefusalCallback" << std::endl;
}

void BpSinkAsync::NextBundleLengthCallback(uint32_t nextBundleLength) {
    std::cout << "next bundle length: " << nextBundleLength << std::endl;
}

void BpSinkAsync::KeepAliveCallback() {
    std::cout << "received keepalive packet" << std::endl;
    // * 2 =>
    //If no message (KEEPALIVE or other) has been received for at least
    //twice the keepalive_interval, then either party MAY terminate the
    //session by transmitting a one-byte SHUTDOWN message (as described in
    //Table 2) and by closing the TCP connection.
    m_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds * 2)); //cancels active timer with cancel flag in callback
    m_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&BpSinkAsync::OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));
}

void BpSinkAsync::ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
                                         bool hasReconnectionDelay, uint32_t reconnectionDelaySeconds)
{
    std::cout << "remote has requested shutdown\n";
    if(hasReasonCode) {
        std::cout << "reason for shutdown: "
                  << ((shutdownReasonCode == SHUTDOWN_REASON_CODES::BUSY) ? "busy" :
                     (shutdownReasonCode == SHUTDOWN_REASON_CODES::IDLE_TIMEOUT) ? "idle timeout" :
                     (shutdownReasonCode == SHUTDOWN_REASON_CODES::VERSION_MISMATCH) ? "version mismatch" :  "unassigned")   << std::endl;
    }
    if(hasReconnectionDelay) {
        std::cout << "requested reconnection delay: " << reconnectionDelaySeconds << " seconds" << std::endl;
    }
    ShutdownAndCloseTcpSocket();
}

void BpSinkAsync::OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        if(m_tcpSocketPtr) {
            std::cout << "error: tcp keepalive timed out.. closing socket\n";
            boost::shared_ptr<std::vector<uint8_t> > shutdownPtr = boost::make_shared<std::vector<uint8_t> >();
            Tcpcl::GenerateShutdownMessage(*shutdownPtr, true, SHUTDOWN_REASON_CODES::IDLE_TIMEOUT, false, 0);
            boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*shutdownPtr),
                                             boost::bind(&BpSinkAsync::HandleTcpSend, this, shutdownPtr,
                                                         boost::asio::placeholders::error,
                                                         boost::asio::placeholders::bytes_transferred, true)); //true => shutdown after data sent
        }

    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void BpSinkAsync::OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        //SEND KEEPALIVE PACKET
        if(m_tcpSocketPtr) {
            boost::shared_ptr<std::vector<uint8_t> > keepAlivePtr = boost::make_shared<std::vector<uint8_t> >();
            Tcpcl::GenerateKeepAliveMessage(*keepAlivePtr);
            boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*keepAlivePtr),
                                             boost::bind(&BpSinkAsync::HandleTcpSend, this, keepAlivePtr,
                                                         boost::asio::placeholders::error,
                                                         boost::asio::placeholders::bytes_transferred, false));

            m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds));
            m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&BpSinkAsync::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void BpSinkAsync::ShutdownAndCloseTcpSocket() {
    if(m_tcpSocketPtr) {
        std::cout << "shutting down connection\n";
        m_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_needToSendKeepAliveMessageTimer.cancel();
    m_noKeepAlivePacketReceivedTimer.cancel();
    m_tcpcl.InitRx(); //reset states
}

}  // namespace hdtn
