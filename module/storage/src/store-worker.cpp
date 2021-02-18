#include <math.h>

#include <iostream>

#include "blosc.h"
#include "message.hpp"
#include "store.hpp"

static void *_launch_wrapper(void *arg) {
    hdtn::storage_worker *worker = (hdtn::storage_worker *)arg;
    return worker->execute(NULL);
}

hdtn::storage_worker::~storage_worker() {
    free(outBuf);
}

void hdtn::storage_worker::init(zmq::context_t *ctx, storageConfig config) {
    zmqContext = ctx;
    root = config.storePath;
    queue = config.worker;
}

void *hdtn::storage_worker::execute(void *arg) {
    zmq::message_t rhdr;
    zmq::message_t rmsg;
    std::cout << "[storage-worker] Worker thread starting up." << std::endl;
    outBuf = (char *)malloc(HDTN_BLOSC_MAXBLOCKSZ);

    zmq::socket_t _worker_sock(*zmqContext, zmq::socket_type::pair);
    _worker_sock.connect(queue.c_str());
    zmq::socket_t *egressSock;
    egressSock = new zmq::socket_t(*zmqContext, zmq::socket_type::push);
    egressSock->bind(HDTN_RELEASE_PATH);
    std::cout << "[storage-worker] Initializing flow store ... " << std::endl;
    common_hdr startup_notify = {
        HDTN_MSGTYPE_IOK,
        0};
    if (!storeFlow.init(root)) {
        startup_notify.type = HDTN_MSGTYPE_IABORT;
        _worker_sock.send(&startup_notify, sizeof(common_hdr));
        return NULL;
    }
    _worker_sock.send(&startup_notify, sizeof(common_hdr));
    std::cout << "[storage-worker] Notified parent that startup is complete." << std::endl;
    while (true) {
        _worker_sock.recv(&rhdr);
        hdtn::flow_stats stats = storeFlow.stats();
        workerStats.flow.disk_wbytes = stats.disk_wbytes;
        workerStats.flow.disk_wcount = stats.disk_wcount;
        workerStats.flow.disk_rbytes = stats.disk_rbytes;
        workerStats.flow.disk_rcount = stats.disk_rcount;

        if (rhdr.size() < sizeof(hdtn::common_hdr)) {
            std::cerr << "[storage-worker] Invalid message format - header size too small (" << rhdr.size() << ")" << std::endl;
            continue;
        }
        hdtn::common_hdr *common = (hdtn::common_hdr *)rhdr.data();
        switch (common->type) {
            case HDTN_MSGTYPE_STORE: {
                _worker_sock.recv(&rmsg);
                hdtn::block_hdr *block = (hdtn::block_hdr *)rhdr.data();
                if (rhdr.size() != sizeof(hdtn::block_hdr)) {
                    std::cerr << "[storage-worker] Invalid message format - header size mismatch (" << rhdr.size() << ")" << std::endl;
                }
                if (rmsg.size() > 100)  //need to fix problem of writing message header as bundles
                {
                    write(block, &rmsg);
                }
                break;
            }
            case HDTN_MSGTYPE_IRELSTART: {
                hdtn::irelease_start_hdr *common = (hdtn::irelease_start_hdr *)rhdr.data();
                releaseData(common->flow_id, common->rate, common->duration, egressSock);
                break;
            }
            case HDTN_MSGTYPE_IRELSTOP: {
                std::cout << "stop releasing data\n";
                break;
            }
        }
    }
    return NULL;
}

void hdtn::storage_worker::write(hdtn::block_hdr *hdr, zmq::message_t *message) {
    int res;
    uint64_t chunks = ceil(message->size() / (double)HDTN_BLOSC_MAXBLOCKSZ);
    for (int i = 0; i < chunks; ++i) {
        res = blosc_compress_ctx(9, 0, 4, message->size(), message->data(), outBuf, HDTN_BLOSC_MAXBLOCKSZ, "lz4", 0, 1);
        storeFlow.write(hdr->flow_id, outBuf, res);
        // std::cerr << "[storage-worker] Appending block (" << rmsg.size() << " raw / " << res
        //std::cerr << "[storage-worker] Appending block (" << message->size() << " raw / " << res
        //  << " compressed / ratio=" << ((double)(res))/message->size()<< ")" << std::endl;
    }
}
void hdtn::storage_worker::releaseData(uint32_t flow, uint64_t rate, uint64_t duration, zmq::socket_t *egressSock) {
    std::cout << "release worker triggered." << std::endl;
    int dataReturned = 0;
    uint64_t totalReturned = 0;
    char ihdr[sizeof(hdtn::block_hdr)];
    hdtn::block_hdr *block = (hdtn::block_hdr *)ihdr;
    memset(ihdr, 0, sizeof(hdtn::block_hdr));
    memset(outBuf, 0, HDTN_BLOSC_MAXBLOCKSZ);

    timeval tv;
    gettimeofday(&tv, NULL);
    double start = (tv.tv_sec + (tv.tv_usec / 1000000.0));

    dataReturned = storeFlow.read(flow, outBuf, HDTN_BLOSC_MAXBLOCKSZ);
    int messageSize = 0;
    int offset = 0;
    while (dataReturned > 0) {
        totalReturned += messageSize;
        char decompressed[HDTN_BLOSC_MAXBLOCKSZ];
        messageSize = blosc_decompress_ctx(outBuf, decompressed, HDTN_BLOSC_MAXBLOCKSZ, 1);

        if (messageSize > 0) {
            block->base.type = HDTN_MSGTYPE_EGRESS;
            block->flow_id = flow;
            egressSock->send(ihdr, sizeof(hdtn::block_hdr), ZMQ_MORE);
            egressSock->send(decompressed, messageSize, 0);
        }
        dataReturned = storeFlow.read(flow, outBuf, HDTN_BLOSC_MAXBLOCKSZ);
    }
    gettimeofday(&tv, NULL);
    double end = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    workerStats.flow.read_rate = ((totalReturned * 8.0) / (1024.0 * 1024)) / (end - start);
    workerStats.flow.read_ts = end - start;
    std::cout << "Total bytes returned: " << totalReturned << ", Mbps released: " << workerStats.flow.read_rate << " in " << workerStats.flow.read_ts << " sec" << std::endl;
    /*hdtn::flow_stats stats = storeFlow.stats();
    workerStats.flow.disk_rbytes=stats.disk_rbytes;
    workerStats.flow.disk_rcount=stats.disk_rcount;
    std::cout << "[storage-worker] " <<  stats.disk_wcount << " w count " << stats.disk_wbytes << " w bytes " << stats.disk_rcount << " r count " << stats.disk_rbytes << " r bytes \n";*/
}

void hdtn::storage_worker::launch() {
    std::cout << "[storage-worker] Launching worker thread ..." << std::endl;
    pthread_create(&storageThread, NULL, _launch_wrapper, this);
}
