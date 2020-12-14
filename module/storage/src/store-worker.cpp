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
    free(_out_buf);
}

void hdtn::storage_worker::init(zmq::context_t *ctx, storage_config config) {
    _ctx = ctx;
    _root = config.store_path;
    _queue = config.worker;
}

void *hdtn::storage_worker::execute(void *arg) {
    zmq::message_t rhdr;
    zmq::message_t rmsg;
    std::cout << "[storage-worker] Worker thread starting up." << std::endl;
    _out_buf = (char *)malloc(HDTN_BLOSC_MAXBLOCKSZ);

    zmq::socket_t _worker_sock(*_ctx, zmq::socket_type::pair);
    _worker_sock.connect(_queue.c_str());
    std::cout << "[storage-worker] Initializing flow store ... " << std::endl;
    common_hdr startup_notify = {
        HDTN_MSGTYPE_IOK,
        0};
    if (!_store.init(_root)) {
        startup_notify.type = HDTN_MSGTYPE_IABORT;
        _worker_sock.send(&startup_notify, sizeof(common_hdr));
        return NULL;
    }
    _worker_sock.send(&startup_notify, sizeof(common_hdr));
    std::cout << "[storage-worker] Notified parent that startup is complete." << std::endl;
    while (true) {
        _worker_sock.recv(&rhdr);
        if (rhdr.size() < sizeof(hdtn::common_hdr)) {
            std::cerr << "[storage-worker] Invalid message format - header size too small (" << rhdr.size() << ")" << std::endl;
            continue;
        }
        hdtn::common_hdr *common = (hdtn::common_hdr *)rhdr.data();
        if (common->type == HDTN_MSGTYPE_STORE) {
            _worker_sock.recv(&rmsg);
            hdtn::block_hdr *block = (hdtn::block_hdr *)rhdr.data();
            if (rhdr.size() != sizeof(hdtn::block_hdr)) {
                std::cerr << "[storage-worker] Invalid message format - header size mismatch (" << rhdr.size() << ")" << std::endl;
            }
            write(block, &rmsg);
        }
    }
    return NULL;
}

void hdtn::storage_worker::write(hdtn::block_hdr *hdr, zmq::message_t *message) {
    // std::cerr << "[storage-worker] Received chunk of size " << rmsg.size() << std::endl;
    uint64_t chunks = ceil(message->size() / (double)HDTN_BLOSC_MAXBLOCKSZ);
    for (int i = 0; i < chunks; ++i) {
        int res = blosc_compress_ctx(9, 0, 4, message->size(), message->data(), _out_buf, HDTN_BLOSC_MAXBLOCKSZ, "lz4", 0, 1);
        _store.write(hdr->flow_id, _out_buf, res);
        //std::cerr << "[storage-worker] Appending block (" << rmsg.size() << " raw / " << res
        //<< " compressed / ratio=" << ((double)(res))/rmsg.size() << ")" << std::endl;
    }
}

void hdtn::storage_worker::launch() {
    std::cout << "[storage-worker] Launching worker thread ..." << std::endl;
    pthread_create(&_thread, NULL, _launch_wrapper, this);
}
