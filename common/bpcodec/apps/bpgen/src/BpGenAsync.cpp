#include <string.h>
#include <iostream>
#include "BpGenAsync.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <time.h>
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

BpGenAsync::BpGenAsync() : m_udpSocket(m_ioService), m_work(m_ioService), m_running(false) {

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

    if (m_udpSocket.is_open()) {
        try {
            m_udpSocket.close();
        } catch (const boost::system::system_error & e) {
            std::cerr << " Error closing udp socket: " << e.what() << std::endl;
        }
    }
    m_tcpclBundleSourcePtr = boost::shared_ptr<TcpclBundleSource>(); //delete it
    m_stcpBundleSourcePtr = boost::shared_ptr<StcpBundleSource>(); //delete it
    if (!m_ioService.stopped()) {
        m_ioService.stop();
    }
    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr = boost::make_shared<boost::thread>();
    }
}

void BpGenAsync::Start(const std::string & hostname, const std::string & port, bool useTcpcl, bool useStcp, uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize, const std::string & thisLocalEidString) {
    if (m_running) {
        std::cerr << "error: BpGenAsync::Start called while BpGenAsync is already running" << std::endl;
        return;
    }

    if (m_ioService.stopped()) {
        m_ioService.reset();
    }

    if(useTcpcl) {
        m_tcpclBundleSourcePtr = boost::make_shared<TcpclBundleSource>(30, thisLocalEidString);
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
        m_stcpBundleSourcePtr = boost::make_shared<StcpBundleSource>(15);
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
        try {
            static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
            boost::asio::ip::udp::resolver resolver(m_ioService);
            m_udpDestinationEndpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), hostname, port, UDP_RESOLVER_FLAGS));

            m_udpSocket.open(boost::asio::ip::udp::v4());
            m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)); //bind to 0 (random ephemeral port)

            std::cout << "UDP Bound on ephemeral port " << m_udpSocket.local_endpoint().port() << std::endl;

        } catch (const boost::system::system_error & e) {
            std::cerr << "Error in BpGenAsync::Start(): " << e.what() << std::endl;
            return;
        }

        m_ioServiceThreadPtr = boost::make_shared<boost::thread>(
            boost::bind(&boost::asio::io_service::run, &m_ioService)); //io service thread only needed for udp.. tcpcl maintains its own io service and thread
    }



    m_running = true;
    m_bpGenThreadPtr = boost::make_shared<boost::thread>(
        boost::bind(&BpGenAsync::BpGenThreadFunc, this, bundleSizeBytes, bundleRate, tcpclFragmentSize)); //create and start the worker thread



}

void BpGenAsync::BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize) {



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
        std::cout << "bundle rate must be non-zero.. exiting"  << std::endl;;
        return;
    }


    //stats?
    //uint64_t bundle_count = 0;
    m_bundleCount = 0;
    uint64_t bundle_data = 0;
    uint64_t raw_data = 0;

    int source_node = BP_GEN_SRC_NODE_DEFAULT;
    int dest_node = BP_GEN_DST_NODE_DEFAULT;

    std::vector<uint8_t> data_buffer(bundleSizeBytes);
    memset(data_buffer.data(), 0, bundleSizeBytes);
    bpgen_hdr* hdr = (bpgen_hdr*)data_buffer.data();

    uint64_t last_time = 0;
    uint64_t curr_time = 0;
    uint64_t seq = 0;
    uint64_t bseq = 0;

    unsigned int numUnackedBundles = 0;

    boost::asio::deadline_timer deadlineTimer(m_ioService, boost::posix_time::microseconds(sValU64));
    boost::shared_ptr<std::vector<uint8_t> > bundleToSend = boost::make_shared<std::vector<uint8_t> >(BP_MSG_BUFSZ);
    while (m_running) { //keep thread alive if running

        {
            boost::system::error_code ec;
            deadlineTimer.wait(ec);
            if(ec) {
                std::cout << "timer error: " << ec.message() << std::endl;
                return;
            }
            deadlineTimer.expires_at(deadlineTimer.expires_at() + boost::posix_time::microseconds(sValU64));
        }
        //boost::this_thread::sleep(boost::posix_time::microseconds(sValU64));

        if (m_udpSocket.is_open()) { //udp requires new data, but the tcpcl forward does a copy before send
            bundleToSend = boost::make_shared<std::vector<uint8_t> >(BP_MSG_BUFSZ);
        }
        else {
            bundleToSend->resize(BP_MSG_BUFSZ);
        }

        {

            char* curr_buf = (char*)bundleToSend->data(); //(msgbuf[idx].msg_hdr.msg_iov->iov_base);
            /*curr_time = time(0);
            if(curr_time == last_time) {
                ++seq;
            }
            else {
                gettimeofday(&tv, NULL);
                double elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
                elapsed -= start;
                start = start + elapsed;
                fprintf(log, "%0.6f, %lu, %lu, %lu, %lu\n", elapsed, bundle_count, raw_data, bundle_data, tsc_total);
                fflush(log);
                bundle_count = 0;
                bundle_data = 0;
                raw_data = 0;
                tsc_total = 0;
                seq = 0;
            }
            last_time = curr_time;
            */
            bpv6_primary_block primary;
            memset(&primary, 0, sizeof(bpv6_primary_block));
            primary.version = 6;
            bpv6_canonical_block block;
            memset(&block, 0, sizeof(bpv6_canonical_block));
            uint64_t bundle_length = 0;
            primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) | bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT);
            primary.src_node = source_node;
            primary.src_svc = 1;
            primary.dst_node = dest_node;
            primary.dst_svc = 1;
            primary.creation = (uint64_t)bpv6_unix_to_5050(curr_time);
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
        else if (m_udpSocket.is_open()) { //udp
            m_udpSocket.async_send_to(boost::asio::buffer(*bundleToSend), m_udpDestinationEndpoint,
                                          boost::bind(&BpGenAsync::HandleUdpSendBundle, this, bundleToSend,
                                                      boost::asio::placeholders::error,
                                                      boost::asio::placeholders::bytes_transferred));
        }

    }

//    std::cout << "bundle_count: " << bundle_count << std::endl;
    std::cout << "bundle_count: " << m_bundleCount << std::endl;
    std::cout << "bundle_data (payload data): " << bundle_data << " bytes" << std::endl;
    std::cout << "raw_data (bundle overhead + payload data): " << raw_data << " bytes" << std::endl;

    std::cout << "BpGenAsync::BpGenThreadFunc thread exiting\n";
}

void BpGenAsync::HandleUdpSendBundle(boost::shared_ptr<std::vector<uint8_t> > vecPtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
}
