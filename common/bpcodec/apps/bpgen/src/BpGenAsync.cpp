#include <string.h>
#include <iostream>
#include "BpGenAsync.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>

#include <time.h>
#include "TimestampUtil.h"
#include "codec/bpv6.h"
#ifndef _WIN32
#include "util/tsc.h"
#endif

#define BP_GEN_SRC_NODE_DEFAULT  (1)
#define BP_GEN_DST_NODE_DEFAULT  (2)
#define BP_GEN_LOGFILE           "bpgen.%lu.csv"

struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};

BpGenAsync::BpGenAsync() : m_running(false) {

}

BpGenAsync::~BpGenAsync() {
    Stop();
}

void BpGenAsync::Stop() {
    m_running = false;
//    boost::this_thread::sleep(boost::posix_time::seconds(1));
    if(m_bpGenThreadPtr) {
        m_bpGenThreadPtr->join();
        m_bpGenThreadPtr.reset(); //delete it
    }

    // Get the final stats
    if (this->m_udpBundleSourcePtr) {
        m_udpBundleSourcePtr->Stop();
        m_FinalStats.m_totalUdpPacketsSent = m_udpBundleSourcePtr->GetTotalUdpPacketsSent();
        m_FinalStats.m_totalUdpPacketsAckedByRate = m_udpBundleSourcePtr->GetTotalUdpPacketsAcked();
        m_FinalStats.m_totalUdpPacketsAckedByUdpSendCallback = m_udpBundleSourcePtr->GetTotalUdpPacketsAcked();
    } else if (this->m_tcpclBundleSourcePtr) {
        m_tcpclBundleSourcePtr->Stop();
        m_FinalStats.m_totalDataSegmentsAcked = m_tcpclBundleSourcePtr->m_totalDataSegmentsAcked;
    } else if (this->m_stcpBundleSourcePtr) {
        m_stcpBundleSourcePtr->Stop();
        m_FinalStats.m_totalDataSegmentsAckedByTcpSendCallback = m_stcpBundleSourcePtr->m_totalDataSegmentsAckedByTcpSendCallback;
    }
    else if (this->m_ltpBundleSourcePtr) {
        m_ltpBundleSourcePtr->Stop();
        m_FinalStats.m_totalDataSegmentsAckedByTcpSendCallback = m_ltpBundleSourcePtr->m_totalDataSegmentsSentSuccessfullyWithAck;
    }

    m_tcpclBundleSourcePtr.reset(); //delete it
    m_stcpBundleSourcePtr.reset(); //delete it
    m_udpBundleSourcePtr.reset(); //delete it
    m_ltpBundleSourcePtr.reset(); //delete it
}

void BpGenAsync::Start(const std::string & hostname, const std::string & port, bool useTcpcl, bool useStcp, bool useLtp, uint32_t bundleSizeBytes, uint32_t bundleRate,
    uint32_t tcpclFragmentSize, const std::string & thisLocalEidString,
    uint64_t thisLtpEngineId, uint64_t remoteLtpEngineId, uint64_t ltpDataSegmentMtu, uint64_t oneWayLightTimeMs, uint64_t oneWayMarginTimeMs, uint64_t clientServiceId,
    unsigned int numLtpUdpRxPacketsCircularBufferSize, unsigned int maxLtpRxUdpPacketSizeBytes,
    uint64_t destFlowId, uint64_t stcpRateBitsPerSec)
{
    if (m_running) {
        std::cerr << "error: BpGenAsync::Start called while BpGenAsync is already running" << std::endl;
        return;
    }

    // Init final stats
    m_FinalStats.useStcp = useStcp;
    m_FinalStats.useTcpcl = useTcpcl;
    m_FinalStats.bundleCount = m_bundleCount;
    m_FinalStats.m_totalUdpPacketsSent = 0;
    m_FinalStats.m_totalUdpPacketsAckedByRate = 0;
    m_FinalStats.m_totalUdpPacketsAckedByUdpSendCallback = 0;
    m_FinalStats.m_totalDataSegmentsAcked = 0;
    m_FinalStats.m_totalDataSegmentsAckedByTcpSendCallback = 0;

    if(useTcpcl) {
        m_tcpclBundleSourcePtr = boost::make_unique<TcpclBundleSource>(30, thisLocalEidString);
        m_tcpclBundleSourcePtr->SetOnSuccessfulAckCallback(boost::bind(&BpGenAsync::OnSuccessfulBundleAck, this));
        m_tcpclBundleSourcePtr->Connect(hostname, port);
        for(unsigned int i = 0; i<10; ++i) {
            std::cout << "Waiting for TCPCL to become ready to forward..." << std::endl;
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            if(m_tcpclBundleSourcePtr->ReadyToForward()) {
                std::cout << "TCPCL ready to forward" << std::endl;
                break;
            }
        }
    }
    else if (useStcp) {
        m_stcpBundleSourcePtr = boost::make_unique<StcpBundleSource>(15);
        m_stcpBundleSourcePtr->SetOnSuccessfulAckCallback(boost::bind(&BpGenAsync::OnSuccessfulBundleAck, this));
        m_stcpBundleSourcePtr->Connect(hostname, port);
        for (unsigned int i = 0; i < 10; ++i) {
            std::cout << "Waiting for STCP to become ready to forward..." << std::endl;
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            if (m_stcpBundleSourcePtr->ReadyToForward()) {
                std::cout << "STCP ready to forward" << std::endl;
                break;
            }
        }
    }
    else if (useLtp) {
        m_ltpBundleSourcePtr = boost::make_unique<LtpBundleSource>(clientServiceId, remoteLtpEngineId, thisLtpEngineId, ltpDataSegmentMtu, 1,
            boost::posix_time::milliseconds(oneWayLightTimeMs), boost::posix_time::milliseconds(oneWayMarginTimeMs),
            0, numLtpUdpRxPacketsCircularBufferSize, maxLtpRxUdpPacketSizeBytes, 100);
        m_ltpBundleSourcePtr->SetOnSuccessfulAckCallback(boost::bind(&BpGenAsync::OnSuccessfulBundleAck, this));
        m_ltpBundleSourcePtr->Connect(hostname, port);
        for (unsigned int i = 0; i < 10; ++i) {
            std::cout << "Waiting for LTP to become ready to forward..." << std::endl;
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            if (m_ltpBundleSourcePtr->ReadyToForward()) {
                std::cout << "LTP ready to forward" << std::endl;
                break;
            }
        }
    }
    else {
        m_udpBundleSourcePtr = boost::make_unique<UdpBundleSource>(stcpRateBitsPerSec);
        m_udpBundleSourcePtr->SetOnSuccessfulAckCallback(boost::bind(&BpGenAsync::OnSuccessfulBundleAck, this));
        m_udpBundleSourcePtr->Connect(hostname, port);
        for (unsigned int i = 0; i < 10; ++i) {
            std::cout << "Waiting for UDP to become ready to forward..." << std::endl;
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            if (m_udpBundleSourcePtr->ReadyToForward()) {
                std::cout << "UDP ready to forward" << std::endl;
                break;
            }
        }
    }



    m_running = true;
    m_bpGenThreadPtr = boost::make_unique<boost::thread>(
        boost::bind(&BpGenAsync::BpGenThreadFunc, this, bundleSizeBytes, bundleRate, tcpclFragmentSize, destFlowId)); //create and start the worker thread



}

void BpGenAsync::OnSuccessfulBundleAck() {
    m_conditionVariableAckReceived.notify_one();
}

void BpGenAsync::BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize, uint64_t destFlowId) {



    #define BP_MSG_BUFSZ             (65536 * 100) //todo

    double sval = 0.0;
    uint64_t sValU64 = 0;
    if(bundleRate) {
        std::cout << "Generating up to " << bundleRate << " bundles / second" << std::endl;
        sval = 1000000.0 / bundleRate;   // sleep val in usec
        ////sval *= BP_MSG_NBUF;
        sValU64 = static_cast<uint64_t>(sval);
        std::cout << "Sleeping for " << sValU64 << " usec between bursts" << std::endl;
    }
    else {
        std::cout << "bundle rate of zero used.. Going as fast as possible by allowing up to 5 unacked bundles"  << std::endl;
    }

    std::size_t numEventsTooManyUnackedBundles = 0;
    //stats?
    //uint64_t bundle_count = 0;
    m_bundleCount = 0;
    uint64_t bundle_data = 0;
    uint64_t raw_data = 0;

    int source_node = BP_GEN_SRC_NODE_DEFAULT;
    //int dest_node = BP_GEN_DST_NODE_DEFAULT;

    std::vector<uint8_t> data_buffer(bundleSizeBytes);
    memset(data_buffer.data(), 0, bundleSizeBytes);
    bpgen_hdr* hdr = (bpgen_hdr*)data_buffer.data();

    //const bool doStepSize = false; //unsafe at the moment due to data_buffer
    uint32_t bundleSizeBytesStep = 100;
    

    uint64_t lastTimeRfc5050 = 0;
    uint64_t currentTimeRfc5050 = 0;
    uint64_t seq = 0;
    uint64_t bseq = 0;



    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    boost::asio::io_service ioService;
    boost::asio::deadline_timer deadlineTimer(ioService, boost::posix_time::microseconds(sValU64));
    std::vector<uint8_t> bundleToSend;
    boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::universal_time();
    while (m_running) { //keep thread alive if running
        /*if (doStepSize) {
            bundleSizeBytes = bundleSizeBytesStep;
            bundleSizeBytesStep += 1;
            if (bundleSizeBytesStep > 1000000) {
                bundleSizeBytesStep = 90000;
            }
        }*/
        /*
        if (HegrTcpclEntryAsync * entryTcpcl = dynamic_cast<HegrTcpclEntryAsync*>(entryIt->second.get())) {
                    const std::size_t numAckedRemaining = entryTcpcl->GetTotalBundlesSent() - entryTcpcl->GetTotalBundlesAcked();
                    while (q.size() > numAckedRemaining) {*/
        
        if(bundleRate) {
            boost::system::error_code ec;
            deadlineTimer.wait(ec);
            if(ec) {
                std::cout << "timer error: " << ec.message() << std::endl;
                return;
            }
            deadlineTimer.expires_at(deadlineTimer.expires_at() + boost::posix_time::microseconds(sValU64));
        }
        else {
            const std::size_t numAckedRemaining = 
                (m_tcpclBundleSourcePtr) ? m_tcpclBundleSourcePtr->GetTotalDataSegmentsUnacked() :
                (m_stcpBundleSourcePtr) ? m_stcpBundleSourcePtr->GetTotalDataSegmentsUnacked() :
                (m_udpBundleSourcePtr) ? m_udpBundleSourcePtr->GetTotalUdpPacketsUnacked() :
                (m_ltpBundleSourcePtr) ? m_ltpBundleSourcePtr->GetTotalDataSegmentsUnacked() : 0;
            const std::size_t numUnackedBundleBytesRemaining = 
                (m_tcpclBundleSourcePtr) ? m_tcpclBundleSourcePtr->GetTotalBundleBytesUnacked() :
                (m_stcpBundleSourcePtr) ? m_stcpBundleSourcePtr->GetTotalBundleBytesUnacked() :
                (m_udpBundleSourcePtr) ? m_udpBundleSourcePtr->GetTotalBundleBytesUnacked() : 0; //TODO, FIGURE OUT WHAT'S APPROPRIATE
            if (numAckedRemaining > 5) {
                ++numEventsTooManyUnackedBundles;
                m_conditionVariableAckReceived.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
                //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                continue;
            }
        }
        //boost::this_thread::sleep(boost::posix_time::microseconds(sValU64));

        
        bundleToSend.resize(bundleSizeBytes + 1000);
        

        {

            char* curr_buf = (char*)bundleToSend.data(); //(msgbuf[idx].msg_hdr.msg_iov->iov_base);
            currentTimeRfc5050 = TimestampUtil::GetSecondsSinceEpochRfc5050(); //curr_time = time(0);
            
            if(currentTimeRfc5050 == lastTimeRfc5050) {
                ++seq;
            }
            else {
                /*gettimeofday(&tv, NULL);
                double elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
                elapsed -= start;
                start = start + elapsed;
                fprintf(log, "%0.6f, %lu, %lu, %lu, %lu\n", elapsed, bundle_count, raw_data, bundle_data, tsc_total);
                fflush(log);
                bundle_count = 0;
                bundle_data = 0;
                raw_data = 0;
                tsc_total = 0;*/
                seq = 0;
            }
            lastTimeRfc5050 = currentTimeRfc5050;
            
            bpv6_primary_block primary;
            memset(&primary, 0, sizeof(bpv6_primary_block));
            primary.version = 6;
            bpv6_canonical_block block;
            memset(&block, 0, sizeof(bpv6_canonical_block));
            uint64_t bundle_length = 0;
            primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) | bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT);
            primary.src_node = source_node;
            primary.src_svc = 1;
            primary.dst_node = destFlowId;
            primary.dst_svc = 1;
            primary.creation = currentTimeRfc5050; //(uint64_t)bpv6_unix_to_5050(curr_time);
            primary.lifetime = 1000;
            primary.sequence = seq;
            ////uint64_t tsc_start = rdtsc();
            bundle_length = bpv6_primary_block_encode(&primary, curr_buf, 0, BP_MSG_BUFSZ);
            ////tsc_total += rdtsc() - tsc_start;
            block.type = BPV6_BLOCKTYPE_PAYLOAD;
            block.flags = BPV6_BLOCKFLAG_LAST_BLOCK;
            block.length = bundleSizeBytes;
            ////tsc_start = rdtsc();
            bundle_length += bpv6_canonical_block_encode(&block, curr_buf, bundle_length, BP_MSG_BUFSZ);
            ////tsc_total += rdtsc() - tsc_start;
#ifndef _WIN32
            hdr->tsc = rdtsc();
            clock_gettime(CLOCK_REALTIME, &hdr->abstime);
#endif // !_WIN32
            hdr->seq = bseq++;
            memcpy(curr_buf + bundle_length, data_buffer.data(), bundleSizeBytes);
            bundle_length += bundleSizeBytes;
            //msgbuf[idx].msg_hdr.msg_iov[0].iov_len = bundle_length;
            //++bundle_count;
            ++m_bundleCount;
            bundle_data += bundleSizeBytes;     // payload data
            raw_data += bundle_length; // bundle overhead + payload data

            bundleToSend.resize(bundle_length);
        }



        //send message
        if(m_tcpclBundleSourcePtr) { //using tcpcl (not udp)
            if (!m_tcpclBundleSourcePtr->Forward(bundleToSend)) {
                m_running = false;
            }
        }
        else if (m_stcpBundleSourcePtr) { //using stcp (not udp)
            if (!m_stcpBundleSourcePtr->Forward(bundleToSend)) {
                m_running = false;
            }
        }
        else if (m_udpBundleSourcePtr) { //udp
            if (!m_udpBundleSourcePtr->Forward(bundleToSend)) {
                m_running = false;
            }
        }
        else if (m_ltpBundleSourcePtr) { //ltp
            if (!m_ltpBundleSourcePtr->Forward(bundleToSend)) {
                m_running = false;
            }
        }

        if (bundleToSend.size() != 0) {
            std::cerr << "error in BpGenAsync::BpGenThreadFunc: bundleToSend was not moved in Forward" << std::endl;
            std::cerr << "bundleToSend.size() : " << bundleToSend.size() << std::endl;
        }

    }
    boost::posix_time::ptime finishedTime = boost::posix_time::microsec_clock::universal_time();

//    std::cout << "bundle_count: " << bundle_count << std::endl;
    std::cout << "bundle_count: " << m_bundleCount << std::endl;
    std::cout << "bundle_data (payload data): " << bundle_data << " bytes" << std::endl;
    std::cout << "raw_data (bundle overhead + payload data): " << raw_data << " bytes" << std::endl;
    if (bundleRate == 0) {
        std::cout << "numEventsTooManyUnackedBundles: " << numEventsTooManyUnackedBundles << std::endl;
    }

    boost::posix_time::time_duration diff = finishedTime - startTime;
    {
        const double rateMbps = (bundle_data * 8.0) / (diff.total_microseconds());
        printf("Sent bundle_data (payload data) at %0.4f Mbits/sec\n", rateMbps);
    }
    {
        const double rateMbps = (raw_data * 8.0) / (diff.total_microseconds());
        printf("Sent raw_data (bundle overhead + payload data) at %0.4f Mbits/sec\n", rateMbps);
    }

    std::cout << "BpGenAsync::BpGenThreadFunc thread exiting\n";
}

std::size_t BpGenAsync::GetTotalBundlesAcked() {
    if(m_tcpclBundleSourcePtr) { //using tcpcl (not udp)
        return m_tcpclBundleSourcePtr->GetTotalDataSegmentsAcked();
    }
    else if (m_stcpBundleSourcePtr) { //using stcp (not udp)
        return m_stcpBundleSourcePtr->GetTotalDataSegmentsAcked();
    }
    else if (m_udpBundleSourcePtr) { //udp
        return m_udpBundleSourcePtr->GetTotalUdpPacketsAcked();
    }
    else if (m_ltpBundleSourcePtr) { //ltp
        return m_ltpBundleSourcePtr->GetTotalDataSegmentsAcked();
    }
    return 0;
}

