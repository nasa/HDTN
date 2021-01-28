#include <math.h>

#include <iostream>

#include "message.hpp"
#include "store.hpp"
#include "BundleStorageManagerMT.h"

hdtn::ZmqStorageInterface::ZmqStorageInterface() : m_running(false) {}

hdtn::ZmqStorageInterface::~ZmqStorageInterface() {
    m_running = false; //thread stopping criteria
    if (m_threadPtr) {
            m_threadPtr->join();
    }
    m_threadPtr = boost::shared_ptr<boost::thread>();

}

void hdtn::ZmqStorageInterface::init(zmq::context_t *ctx, const storageConfig & config) {
    m_zmqContextPtr = ctx;
    m_root = config.storePath;
    m_queue = config.worker;
}

static void Write(hdtn::block_hdr *hdr, zmq::message_t *message, BundleStorageManagerMT & bsm) {
    //res = blosc_compress_ctx(9, 0, 4, message->size(), message->data(), outBuf, HDTN_BLOSC_MAXBLOCKSZ, "lz4", 0, 1);
        //storeFlow.write(hdr->flow_id, outBuf, res);

    const boost::uint64_t size = message->size();
    boost::uint8_t * data = (boost::uint8_t *) message->data();


    const unsigned int linkId = hdr->flow_id;
    const unsigned int priorityIndex = 0; //could be 0, 1, or 2, but keep 0 for fifo mode
    static abs_expiration_t bundleI = 0;
    const abs_expiration_t absExpiration = bundleI++; //increment this every time for fifo mode

    BundleStorageManagerSession_WriteToDisk sessionWrite;
    bp_primary_if_base_t bundleMetaData;
    bundleMetaData.flags = (priorityIndex & 3) << 7;
    bundleMetaData.dst_node = linkId;
    bundleMetaData.length = size;
    bundleMetaData.creation = 0;
    bundleMetaData.lifetime = absExpiration;

    boost::uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, bundleMetaData);
    //std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
    if (totalSegmentsRequired == 0) {
        std::cerr << "out of space\n";
        return;
    }
    //totalSegmentsStoredOnDisk += totalSegmentsRequired;
    //totalBytesWrittenThisTest += size;

    for (boost::uint64_t i = 0; i < totalSegmentsRequired; ++i) {
        std::size_t bytesToCopy = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
        if (i == totalSegmentsRequired - 1) {
            boost::uint64_t modBytes = (size % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
            if (modBytes != 0) {
                bytesToCopy = modBytes;
            }
        }

        bsm.PushSegment(sessionWrite, &data[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE], bytesToCopy);
    }
}

static void ReleaseData(uint32_t flow, uint64_t rate, uint64_t duration, zmq::socket_t *egressSock, BundleStorageManagerMT & bsm) {
    std::cout << "release worker triggered." << std::endl;
    //int dataReturned = 0;
    //uint64_t totalReturned = 0;
    char ihdr[sizeof(hdtn::block_hdr)];
    hdtn::block_hdr *block = (hdtn::block_hdr *)ihdr;
    memset(ihdr, 0, sizeof(hdtn::block_hdr));
    const boost::uint64_t largestBundleSize = HDTN_BLOSC_MAXBLOCKSZ * 2;
    std::vector<boost::uint8_t> bundleReadBack(largestBundleSize, 0); //initialize it to zero

    //timeval tv;
    //gettimeofday(&tv, NULL);
    //double start = (tv.tv_sec + (tv.tv_usec / 1000000.0));

    unsigned int numBundlesReadBack = 0;
    while(1) {


            std::vector<boost::uint64_t> availableDestLinks = { flow }; //fifo mode, only put in the one link you want to read (could be more tho)


            //std::cout << "reading\n";
            BundleStorageManagerSession_ReadFromDisk sessionRead;
            boost::uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
            //std::cout << "bytesToReadFromDisk " << bytesToReadFromDisk << "\n";
            if (bytesToReadFromDisk == 0) { //no more of these links to read
                    break;
            }
            else {//this link has a bundle in the fifo


                    //USE THIS COMMENTED OUT CODE IF YOU DECIDE YOU DON'T WANT TO READ THE BUNDLE AFTER PEEKING AT IT (MAYBE IT'S TOO BIG RIGHT NOW)
                    //return top then take out again
                    //bsm.ReturnTop(sessionRead);
                    //bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks); //get it back


                    const std::size_t numSegmentsToRead = sessionRead.chainInfo.second.size();
                    std::size_t totalBytesRead = 0;
                    for (std::size_t i = 0; i < numSegmentsToRead; ++i) {
                            totalBytesRead += bsm.TopSegment(sessionRead, &bundleReadBack[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE]);
                    }
                    //std::cout << "totalBytesRead " << totalBytesRead << "\n";
                    if(totalBytesRead != bytesToReadFromDisk){
                        std::cout << "error: totalBytesRead != bytesToReadFromDisk\n";
                    }




                    //if you're happy with the bundle data you read back, then officially remove it from the disk
                    bool successRemoveBundle = bsm.RemoveReadBundleFromDisk(sessionRead);
                    if(!successRemoveBundle) {
                        std::cout << "error freeing bundle from disk\n";
                    }
                    ++numBundlesReadBack;
                    block->base.type = HDTN_MSGTYPE_EGRESS;
                    block->flow_id = flow;
                    egressSock->send(ihdr, sizeof(hdtn::block_hdr), ZMQ_MORE);
                    egressSock->send(bundleReadBack.data(), bytesToReadFromDisk, 0);
            }


    }



    //gettimeofday(&tv, NULL);
    //double end = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    std::cout << "numBundlesReadBack = " << numBundlesReadBack << std::endl;
    //workerStats.flow.read_rate = ((totalReturned * 8.0) / (1024.0 * 1024)) / (end - start);
    //workerStats.flow.read_ts = end - start;
    //std::cout << "Total bytes returned: " << totalReturned << ", Mbps released: " << workerStats.flow.read_rate << " in " << workerStats.flow.read_ts << " sec" << std::endl;
    /*hdtn::flow_stats stats = storeFlow.stats();
    workerStats.flow.disk_rbytes=stats.disk_rbytes;
    workerStats.flow.disk_rcount=stats.disk_rcount;
    std::cout << "[storage-worker] " <<  stats.disk_wcount << " w count " << stats.disk_wbytes << " w bytes " << stats.disk_rcount << " r count " << stats.disk_rbytes << " r bytes \n";*/
}

void hdtn::ZmqStorageInterface::ThreadFunc() {
    zmq::message_t rhdr;
    zmq::message_t rmsg;
    std::cout << "[storage-worker] Worker thread starting up." << std::endl;

    zmq::socket_t workerSock(*m_zmqContextPtr, zmq::socket_type::pair);
    workerSock.connect(m_queue.c_str());
    zmq::socket_t egressSock(*m_zmqContextPtr, zmq::socket_type::push);
    egressSock.bind(HDTN_RELEASE_PATH);
    std::cout << "[ZmqStorageInterface] Initializing BundleStorageManagerMT ... " << std::endl;
    common_hdr startupNotify = {
        HDTN_MSGTYPE_IOK,
        0};
    BundleStorageManagerMT bsm;
    bsm.Start();
    //if (!m_storeFlow.init(m_root)) {
    //    startupNotify.type = HDTN_MSGTYPE_IABORT;
    //    workerSock.send(&startupNotify, sizeof(common_hdr));
    //    return;
    //}
    workerSock.send(&startupNotify, sizeof(common_hdr));
    std::cout << "[ZmqStorageInterface] Notified parent that startup is complete." << std::endl;
    while (m_running) {
        workerSock.recv(&rhdr);
        /*hdtn::flow_stats stats = m_storeFlow.stats();
        m_workerStats.flow.disk_wbytes = stats.disk_wbytes;
        m_workerStats.flow.disk_wcount = stats.disk_wcount;
        m_workerStats.flow.disk_rbytes = stats.disk_rbytes;
        m_workerStats.flow.disk_rcount = stats.disk_rcount;*/

        if (rhdr.size() < sizeof(hdtn::common_hdr)) {
            std::cerr << "[storage-worker] Invalid message format - header size too small (" << rhdr.size() << ")" << std::endl;
            continue;
        }
        hdtn::common_hdr commonHdr;
        memcpy(&commonHdr, rhdr.data(), sizeof(hdtn::common_hdr));
        switch (commonHdr.type) {
            case HDTN_MSGTYPE_STORE: {
                workerSock.recv(&rmsg);
                hdtn::block_hdr *block = (hdtn::block_hdr *)rhdr.data();
                if (rhdr.size() != sizeof(hdtn::block_hdr)) {
                    std::cerr << "[storage-worker] Invalid message format - header size mismatch (" << rhdr.size() << ")" << std::endl;
                }
                if (rmsg.size() > 100)  //need to fix problem of writing message header as bundles
                {
                    Write(block, &rmsg, bsm);
                }
                break;
            }
            case HDTN_MSGTYPE_IRELSTART: {
                hdtn::irelease_start_hdr iReleaseStartHdr;
                memcpy(&iReleaseStartHdr, rhdr.data(), sizeof(hdtn::irelease_start_hdr));
                ReleaseData(iReleaseStartHdr.flow_id, iReleaseStartHdr.rate, iReleaseStartHdr.duration, &egressSock, bsm);
                break;
            }
            case HDTN_MSGTYPE_IRELSTOP: {
                std::cout << "stop releasing data\n";
                break;
            }
        }
    }
}




void hdtn::ZmqStorageInterface::launch() {
    if (!m_running) {
        m_running = true;
        std::cout << "[ZmqStorageInterface] Launching worker thread ..." << std::endl;
        m_threadPtr = boost::make_shared<boost::thread>(
                boost::bind(&ZmqStorageInterface::ThreadFunc, this)); //create and start the worker thread
    }
}
