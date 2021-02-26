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
    egressSock->bind(HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH); //TODO: bind is probably incorrect here
    std::cout << "[storage-worker] Initializing flow store ... " << std::endl;
    hdtn::CommonHdr startup_notify = {
        HDTN_MSGTYPE_IOK,
        0};
    if (!storeFlow.init(root)) {
        startup_notify.type = HDTN_MSGTYPE_IABORT;
        _worker_sock.send(&startup_notify, sizeof(hdtn::CommonHdr));
        return NULL;
    }
    _worker_sock.send(&startup_notify, sizeof(hdtn::CommonHdr));
    std::cout << "[storage-worker] Notified parent that startup is complete." << std::endl;
    while (true) {
        _worker_sock.recv(&rhdr);
        hdtn::FlowStats stats = storeFlow.stats();
        workerStats.flow.diskWbytes = stats.diskWbytes;
        workerStats.flow.diskWcount = stats.diskWcount;
        workerStats.flow.diskRbytes = stats.diskRbytes;
        workerStats.flow.diskRcount = stats.diskRcount;

        if (rhdr.size() < sizeof(hdtn::CommonHdr)) {
            std::cerr << "[storage-worker] Invalid message format - header size too small (" << rhdr.size() << ")" << std::endl;
            continue;
        }
        hdtn::CommonHdr *common = (hdtn::CommonHdr *)rhdr.data();
        switch (common->type) {
            case HDTN_MSGTYPE_STORE: {
                _worker_sock.recv(&rmsg);
                hdtn::BlockHdr *block = (hdtn::BlockHdr *)rhdr.data();
                if (rhdr.size() != sizeof(hdtn::BlockHdr)) {
                    std::cerr << "[storage-worker] Invalid message format - header size mismatch (" << rhdr.size() << ")" << std::endl;
                }
                if (rmsg.size() > 100)  //need to fix problem of writing message header as bundles
                {
                    write(block, &rmsg);
                }
                break;
            }
            case HDTN_MSGTYPE_IRELSTART: {
                hdtn::IreleaseStartHdr *common = (hdtn::IreleaseStartHdr *)rhdr.data();
                releaseData(common->flowId, common->rate, common->duration, egressSock);
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

void hdtn::storage_worker::write(hdtn::BlockHdr *hdr, zmq::message_t *message) {
    int res;
    uint64_t chunks = ceil(message->size() / (double)HDTN_BLOSC_MAXBLOCKSZ);
    for (int i = 0; i < chunks; ++i) {
        res = blosc_compress_ctx(9, 0, 4, message->size(), message->data(), outBuf, HDTN_BLOSC_MAXBLOCKSZ, "lz4", 0, 1);
        storeFlow.write(hdr->flowId, outBuf, res);
        // std::cerr << "[storage-worker] Appending block (" << rmsg.size() << " raw / " << res
        //std::cerr << "[storage-worker] Appending block (" << message->size() << " raw / " << res
        //  << " compressed / ratio=" << ((double)(res))/message->size()<< ")" << std::endl;
    }
}
void hdtn::storage_worker::releaseData(uint32_t flow, uint64_t rate, uint64_t duration, zmq::socket_t *egressSock) {
    std::cout << "release worker triggered." << std::endl;
    int dataReturned = 0;
    uint64_t totalReturned = 0;
    char ihdr[sizeof(hdtn::BlockHdr)];
    hdtn::BlockHdr *block = (hdtn::BlockHdr *)ihdr;
    memset(ihdr, 0, sizeof(hdtn::BlockHdr));
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
            block->flowId = flow;
            egressSock->send(ihdr, sizeof(hdtn::BlockHdr), ZMQ_MORE);
            egressSock->send(decompressed, messageSize, 0);
        }
        dataReturned = storeFlow.read(flow, outBuf, HDTN_BLOSC_MAXBLOCKSZ);
    }
    gettimeofday(&tv, NULL);
    double end = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    ////workerStats.flow.read_rate = ((totalReturned * 8.0) / (1024.0 * 1024)) / (end - start);
    ////workerStats.flow.read_ts = end - start;
    std::cout << "Total bytes returned: " << totalReturned << std::endl;// ", Mbps released: " << workerStats.flow.read_rate << " in " << workerStats.flow.read_ts << " sec" << std::endl;
    /*hdtn::flow_stats stats = storeFlow.stats();
    workerStats.flow.disk_rbytes=stats.disk_rbytes;
    workerStats.flow.disk_rcount=stats.disk_rcount;
    std::cout << "[storage-worker] " <<  stats.disk_wcount << " w count " << stats.disk_wbytes << " w bytes " << stats.disk_rcount << " r count " << stats.disk_rbytes << " r bytes \n";*/
}

void hdtn::storage_worker::launch() {
    std::cout << "[storage-worker] Launching worker thread ..." << std::endl;
    pthread_create(&storageThread, NULL, _launch_wrapper, this);
}
