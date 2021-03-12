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
    if(m_bpGenThreadPtr) {
        m_bpGenThreadPtr->join();
        m_bpGenThreadPtr = boost::make_shared<boost::thread>();
    }
    //prevent bpgen from exiting before all bundles sent and acked
    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);
    std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
    for (unsigned int attempt = 0; attempt < 10; ++attempt) {
        const std::size_t numUnacked =
            (m_tcpclBundleSourcePtr) ? m_tcpclBundleSourcePtr->GetTotalDataSegmentsUnacked() :
            (m_stcpBundleSourcePtr) ? m_stcpBundleSourcePtr->GetTotalDataSegmentsUnacked() :
            (m_udpBundleSourcePtr) ? m_udpBundleSourcePtr->GetTotalUdpPacketsUnacked() : 0;
        if (numUnacked) {
            std::cout << "notice: BpGenAsync destructor waiting on " << numUnacked << " unacked bundles" << std::endl;
            if (previousUnacked > numUnacked) {
                previousUnacked = numUnacked;
                attempt = 0;
            }
            m_conditionVariableAckReceived.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        break;
    }

    m_tcpclBundleSourcePtr = boost::shared_ptr<TcpclBundleSource>(); //delete it
    m_stcpBundleSourcePtr = boost::shared_ptr<StcpBundleSource>(); //delete it
    m_udpBundleSourcePtr.reset(); //delete it
}

void BpGenAsync::Start(const std::string & hostname, const std::string & port, bool useTcpcl, bool useStcp, uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize, const std::string & thisLocalEidString, uint64_t destFlowId, uint64_t stcpRateBitsPerSec) {
    if (m_running) {
        std::cerr << "error: BpGenAsync::Start called while BpGenAsync is already running" << std::endl;
        return;
    }

    if(useTcpcl) {
        m_tcpclBundleSourcePtr = boost::make_shared<TcpclBundleSource>(30, thisLocalEidString);
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
        m_stcpBundleSourcePtr = boost::make_shared<StcpBundleSource>(15, stcpRateBitsPerSec);
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
    m_bpGenThreadPtr = boost::make_shared<boost::thread>(
        boost::bind(&BpGenAsync::BpGenThreadFunc, this, bundleSizeBytes, bundleRate, tcpclFragmentSize, destFlowId)); //create and start the worker thread



}

void BpGenAsync::OnSuccessfulBundleAck() {
    m_conditionVariableAckReceived.notify_one();
}

void BpGenAsync::BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize, uint64_t destFlowId) {



    #define BP_MSG_BUFSZ             (65536)

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

    uint64_t lastTimeRfc5050 = 0;
    uint64_t currentTimeRfc5050 = 0;
    uint64_t seq = 0;
    uint64_t bseq = 0;

    unsigned int numUnackedBundles = 0;

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    boost::asio::io_service ioService;
    boost::asio::deadline_timer deadlineTimer(ioService, boost::posix_time::microseconds(sValU64));
    boost::shared_ptr<std::vector<uint8_t> > bundleToSend = boost::make_shared<std::vector<uint8_t> >(BP_MSG_BUFSZ);
    while (m_running) { //keep thread alive if running
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
                (m_udpBundleSourcePtr) ? m_udpBundleSourcePtr->GetTotalUdpPacketsUnacked() : 0;
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

        
        bundleToSend->resize(BP_MSG_BUFSZ);
        

        {

            char* curr_buf = (char*)bundleToSend->data(); //(msgbuf[idx].msg_hdr.msg_iov->iov_base);
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

            bundleToSend->resize(bundle_length);
        }



        //send message
        if(m_tcpclBundleSourcePtr) { //using tcpcl (not udp)
            if (!m_tcpclBundleSourcePtr->Forward(bundleToSend->data(), bundleToSend->size(), numUnackedBundles)) {
                m_running = false;
            }
        }
        else if (m_stcpBundleSourcePtr) { //using stcp (not udp)
            if (!m_stcpBundleSourcePtr->Forward(bundleToSend->data(), bundleToSend->size(), numUnackedBundles)) {
                m_running = false;
            }
        }
        else if (m_udpBundleSourcePtr) { //udp
            if (!m_udpBundleSourcePtr->Forward(bundleToSend->data(), bundleToSend->size(), numUnackedBundles)) {
                m_running = false;
            }
        }

    }

//    std::cout << "bundle_count: " << bundle_count << std::endl;
    std::cout << "bundle_count: " << m_bundleCount << std::endl;
    std::cout << "bundle_data (payload data): " << bundle_data << " bytes" << std::endl;
    std::cout << "raw_data (bundle overhead + payload data): " << raw_data << " bytes" << std::endl;
    if (bundleRate == 0) {
        std::cout << "numEventsTooManyUnackedBundles: " << numEventsTooManyUnackedBundles << std::endl;
    }

    std::cout << "BpGenAsync::BpGenThreadFunc thread exiting\n";
}
