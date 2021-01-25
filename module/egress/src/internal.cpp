#include <arpa/inet.h>
#include <string.h>

#include "egress.h"

using namespace hdtn;

hegr_entry *hegr_manager::_entry(int offset) {
    return (hegr_entry *)(((uint8_t *)_entries) + (offset * HEGR_ENTRY_SZ));
}

hegr_manager::~hegr_manager() {
    int shutdown_status;
    for (int i = 0; i < HEGR_ENTRY_COUNT; ++i) {
        _entry(i)->shutdown();
        delete (_entry(i));
    }
    free(_entries);
}
void hegr_manager::init() {
    bundle_count = 0;
    bundle_data = 0;
    message_count = 0;
    elapsed = 0;
    _entries = malloc(HEGR_ENTRY_SZ * HEGR_ENTRY_COUNT);
    for (int i = 0; i < HEGR_ENTRY_COUNT; ++i) {
        hegr_entry *tmp = new (_entry(i)) hegr_entry;
        tmp->label(i);
    }
    //socket for cut-through mode straight to egress
    //might not actually need two sockets
    zmqCutThroughCtx = new zmq::context_t;
    zmqCutThroughSock = new zmq::socket_t(*zmqCutThroughCtx, zmq::socket_type::pull);
    zmqCutThroughSock->connect(cutThroughAddress);
    //socket for getting bundles fom storage
    zmqReleaseCtx = new zmq::context_t;
    zmqReleaseSock = new zmq::socket_t(*zmqReleaseCtx, zmq::socket_type::pull);
    zmqReleaseSock->connect(ReleaseAddress);
}

void hegr_manager::update() {
    zmq::pollitem_t items[] = {
        {zmqCutThroughSock->handle(),
         0,
         ZMQ_POLLIN,
         0},
        {zmqReleaseSock->handle(),
         0,
         ZMQ_POLLIN,
         0},
    };
    zmq::poll(&items[0], 2, 0);
    if (items[0].revents & ZMQ_POLLIN) {
        dispatch(zmqCutThroughSock);
    }
    if (items[1].revents & ZMQ_POLLIN) {
        dispatch(zmqReleaseSock);
    }
}

void hegr_manager::dispatch(zmq::socket_t *zmqSock) {
    zmq::message_t hdr;
    zmq::message_t message;
    zmqSock->recv(&hdr);
    char bundle[HMSG_MSG_MAX];
    int bundle_size = 0;
    if (hdr.size() < sizeof(hdtn::common_hdr)) {
        std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
        return;
    }
    hdtn::common_hdr *common = (hdtn::common_hdr *)hdr.data();
    hdtn::block_hdr *block = (hdtn::block_hdr *)common;
    switch (common->type) {
        case HDTN_MSGTYPE_EGRESS:
            zmqSock->recv(&message);
            bundle_size = message.size();
            //need to fix problem of message header being read as the bundle
            if (bundle_size > 100) {
                memcpy(bundle, message.data(), bundle_size);
                forward(1, bundle, bundle_size);
                bundle_data += bundle_size;
                bundle_count++;
            }
            break;
    }
}

int hegr_manager::add(int fec, uint64_t flags, const char *dst, int port) {
    struct sockaddr_in saddr;
    saddr.sin_port = htons((uint16_t)port);
    saddr.sin_family = AF_INET;
    int conversion_status;
    conversion_status = inet_pton(AF_INET, dst, &(saddr.sin_addr));
    if (conversion_status != 1) {
        printf("Failure to convert IP address from text to binary");
        return 0;
    }
    if (flags & HEGR_FLAG_STCPv1) {
        hegr_stcp_entry *tcp = new (_entry(fec)) hegr_stcp_entry;
        tcp->init(&saddr, flags);
        tcp->disable();
        return 1;
    } else if (flags & HEGR_FLAG_UDP) {
        hegr_udp_entry *udp = new (_entry(fec)) hegr_udp_entry;
        udp->init(&saddr, flags);
        udp->disable();
        return 1;
    } else {
        return -HDTN_MSGTYPE_ENOTIMPL;
    }

    return 0;
}

void hegr_manager::down(int fec) {
    _entry(fec)->disable();
}

void hegr_manager::up(int fec) {
    _entry(fec)->enable();
}

/** Leaving function for now. Need to know if these sockets will be removed throughout running the code.
int hegr_manager::remove(int fec) {
    int shutdown_status;
    shutdown_status = _entry(fec)->shutdown();
    delete _entry(fec);
    return 0;
}
**/
int hegr_manager::forward(int fec, char *msg, int sz) {
    return _entry(fec)->forward((char **)(&msg), &sz, 1);
}

hegr_entry::hegr_entry() {
    _flags = 0;
    //_next = NULL;
}

void hegr_entry::init(sockaddr_in *inaddr, uint64_t flags) {
}

bool hegr_entry::available() {
    return (_flags & HEGR_FLAG_ACTIVE) && (_flags & HEGR_FLAG_UP);
}

int hegr_entry::disable() {
    return -1;
}

void hegr_entry::rate(uint64_t rate) {
    //_rate = rate;
}

void hegr_entry::label(uint64_t label) {
    _label = label;
}

void hegr_entry::name(char *n) {
    //strncpy(_name, n, HEGR_NAME_SZ);
}

int hegr_entry::enable() {
    return -1;
}

void hegr_entry::update(uint64_t delta) {
    return;
}

int hegr_entry::forward(char **msg, int *sz, int count) {
    return 0;
}

void hegr_entry::shutdown() {
}
