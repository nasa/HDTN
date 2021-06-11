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

    m_outductManager.StopAllOutducts();
    if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
        outduct->GetOutductFinalStats(m_outductFinalStats);
    }
    
}

void BpGenAsync::Start(const OutductsConfig & outductsConfig, uint32_t bundleSizeBytes, uint32_t bundleRate, uint64_t destFlowId) {
    if (m_running) {
        std::cerr << "error: BpGenAsync::Start called while BpGenAsync is already running" << std::endl;
        return;
    }

    if (!m_outductManager.LoadOutductsFromConfig(outductsConfig)) {
        return;
    }

    for (unsigned int i = 0; i <= 10; ++i) {
        std::cout << "Waiting for Outduct to become ready to forward..." << std::endl;
        boost::this_thread::sleep(boost::posix_time::milliseconds(500));
        if (m_outductManager.AllReadyToForward()) {
            std::cout << "Outduct ready to forward" << std::endl;
            break;
        }
        if (i == 10) {
            std::cerr << "Bpgen Outduct unable to connect" << std::endl;
            return;
        }
    }
   


    m_running = true;
    m_bpGenThreadPtr = boost::make_unique<boost::thread>(
        boost::bind(&BpGenAsync::BpGenThreadFunc, this, bundleSizeBytes, bundleRate, destFlowId)); //create and start the worker thread



}


void BpGenAsync::BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint64_t destFlowId) {



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
        if (!m_outductManager.Forward_Blocking(destFlowId, bundleToSend, 3)) {
            std::cerr << "bpgen was unable to send a bundle for 3 seconds.. exiting" << std::endl;
            m_running = false;
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

    if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
        if (outduct->GetConvergenceLayerName() == "ltp_over_udp") {
            std::cout << "Bpgen Keeping UDP open for 4 seconds to acknowledge report segments" << std::endl;
            boost::this_thread::sleep(boost::posix_time::seconds(4));
        }
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
