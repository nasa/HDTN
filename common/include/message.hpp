#ifndef _HDTN_MSG_H
#define _HDTN_MSG_H

#include <stdint.h>

#include "stats.hpp"
#include "codec/bpv6.h"

#define HMSG_MSG_MAX (65536)
#define CHUNK_SIZE (65536 * 100) //TODO

#define HDTN_FLAG_CUSTODY_REQ (0x01)
#define HDTN_FLAG_CUSTODY_OK (0x02)
#define HDTN_FLAG_CUSTODY_FAIL (0x04)

// Common message types shared by all components
#define HDTN_MSGTYPE_EGRESS (0x0004)
#define HDTN_MSGTYPE_STORE (0x0005)

// Egress Messages range is 0xE000 to 0xEAFF
#define HDTN_MSGTYPE_ENOTIMPL (0xE000)  // convergence layer type not  // implemented

// Command and control messages accepted by the storage component - range is
// 0xF000 to 0xFAFF
#define HDTN_MSGTYPE_COK (0xF000)         // acknowledgement that previous command was processed successfully
#define HDTN_MSGTYPE_CFAIL (0xF001)       // negative acknowledgement of previous command
#define HDTN_MSGTYPE_CTELEM_REQ (0xF002)  // request for telemetry from the application
#define HDTN_MSGTYPE_CSCHED_REQ (0xF003)  // request for a scheduled event

// Telemetry messages - range is 0xFB00 to 0xFBFF
#define HDTN_MSGTYPE_TSTORAGE (0xFB00)  // response that indicates telemetry is of type "storage"

// Internal messages used only by the storage component - types start at 0xFC00
#define HDTN_MSGTYPE_IOK (0xFC00)  // indicates successful worker startup
#define HDTN_MSGTYPE_IABORT \
    (0xFC01)  // indicates that the worker encountered a critical failure and will // immediately terminate
#define HDTN_MSGTYPE_ISHUTDOWN (0xFC02)   // tells the worker to shut down
#define HDTN_MSGTYPE_IRELSTART (0xFC03)   // tells the worker to begin releasing data for forwarding
#define HDTN_MSGTYPE_IRELSTOP (0xFC04)    // tells the worker to stop releasing data
#define HDTN_MSGTYPE_IPRELOAD (0xFC05)    // preloads data because an event is scheduled to begin soon
#define HDTN_MSGTYPE_IWORKSTATS (0xFC06)  // update on worker stats sent from worker to parent

#define HDTN_MSGTYPE_EGRESS_TRANSFERRED_CUSTODY (0x5555)

namespace hdtn {
//#pragma pack (push, 1)
struct CommonHdr {
    uint16_t type;
    uint16_t flags;

    bool operator==(const CommonHdr & o) const {
        return (type == o.type) && (flags == o.flags);
    }
};// __attribute__((packed));

struct BlockHdr {
    CommonHdr base;
    uint32_t flowId;
    uint64_t ts;
    uint32_t ttl;
    uint32_t zframe;
    uint64_t bundleSeq;

    bool operator==(const BlockHdr & o) const {
        return (base == o.base) && (flowId == o.flowId) && (ts == o.ts) && (ttl == o.ttl) && (zframe == o.zframe) && (bundleSeq == o.bundleSeq);
    }
};// __attribute__((packed));

struct ToEgressHdr {
    CommonHdr base;
    uint8_t hasCustody;
    uint8_t unused2;
    uint8_t unused3;
    uint8_t unused4;
    cbhe_eid_t finalDestEid;
    uint64_t custodyId;

    //bool operator==(const ToEgressHdr & o) const {
    //    return (base == o.base) && (custodyId == o.custodyId);
    //}
};

struct EgressAckHdr {
    CommonHdr base;
    uint8_t error;
    uint8_t deleteNow; //set if message does not request custody (can be deleted after egress sends it)
    uint8_t unused1;
    uint8_t unused2;
    cbhe_eid_t finalDestEid;
    uint64_t custodyId;

    //bool operator==(const EgressAckHdr & o) const {
    //    return (base == o.base) && (custodyId == o.custodyId);
    //}
};

struct StoreHdr {
    BlockHdr base;
};// __attribute__((packed));

struct TelemStorageHdr {
    CommonHdr base;
    StorageStats stats;
};// __attribute__((packed));

struct CscheduleHdr {
    CommonHdr base;
    uint32_t flowId;   // flow ID
    uint64_t rate;      // bytes / sec
    uint64_t offset;    // msec
    uint64_t duration;  // msec
};// __attribute__((packed));

struct IreleaseStartHdr {
    CommonHdr base;
    cbhe_eid_t finalDestinationEid;   // formerly flow ID
    uint64_t rate;      // bytes / sec
    uint64_t duration;  // msec
};// __attribute__((packed));

struct IreleaseStopHdr {
    CommonHdr base;
    cbhe_eid_t finalDestinationEid;
};// __attribute__((packed));
};  // namespace hdtn
//#pragma pack (pop)
#endif
