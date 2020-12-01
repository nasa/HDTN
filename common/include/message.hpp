#ifndef _HDTN3_MSG_H
#define _HDTN3_MSG_H

#include <stdint.h>
#include "stats.hpp"

#define HMSG_MSG_MAX                  (65536)
#define CHUNK_SIZE  		      (32800)

#define HDTN3_FLAG_CUSTODY_REQ        (0x01)
#define HDTN3_FLAG_CUSTODY_OK         (0x02)
#define HDTN3_FLAG_CUSTODY_FAIL       (0x04)

// Common message types shared by all components
#define HDTN3_MSGTYPE_STORE           (0x0005)

//Egress Messages range is 0xE000 to 0xEAFF
#define HDTN3_MSGTYPE_ENOTIMPL        (0xE000)//convergence layer type not implemented

// Command and control messages accepted by the storage component - range is 0xF000 to 0xFAFF
#define HDTN3_MSGTYPE_COK             (0xF000)  // acknowledgement that previous command was processed successfully
#define HDTN3_MSGTYPE_CFAIL           (0xF001)  // negative acknowledgement of previous command
#define HDTN3_MSGTYPE_CTELEM_REQ      (0xF002)  // request for telemetry from the application
#define HDTN3_MSGTYPE_CSCHED_REQ      (0xF003)  // request for a scheduled event

// Telemetry messages - range is 0xFB00 to 0xFBFF
#define HDTN3_MSGTYPE_TSTORAGE        (0xFB00)  // response that indicates telemetry is of type "storage"

// Internal messages used only by the storage component - types start at 0xFC00
#define HDTN3_MSGTYPE_IOK             (0xFC00)  // indicates successful worker startup
#define HDTN3_MSGTYPE_IABORT          (0xFC01)  // indicates that the worker encountered a critical failure and will immediately terminate
#define HDTN3_MSGTYPE_ISHUTDOWN       (0xFC02)  // tells the worker to shut down
#define HDTN3_MSGTYPE_IRELSTART       (0xFC03)  // tells the worker to begin releasing data for forwarding
#define HDTN3_MSGTYPE_IRELSTOP        (0xFC04)  // tells the worker to stop releasing data
#define HDTN3_MSGTYPE_IPRELOAD        (0xFC05)  // preloads data because an event is scheduled to begin soon
#define HDTN3_MSGTYPE_IWORKSTATS      (0xFC06)  // update on worker stats sent from worker to parent 

namespace hdtn3 {

    struct common_hdr {
        uint16_t type;
        uint16_t flags;        
    } __attribute__((packed));

    struct block_hdr {
        common_hdr  base;
        uint32_t    flow;
        uint64_t    ts;
        uint32_t    ttl;
        uint32_t    zframe;
        uint64_t    bundle_seq;
    } __attribute__((packed));

    struct store_hdr {
        block_hdr   base;
    } __attribute__((packed));

    struct telem_storage_hdr {
        common_hdr     base;
        storage_stats  stats;
    } __attribute__((packed));

    struct cschedule_hdr {
        common_hdr   base;
        uint32_t     flow;      // flow ID
        uint64_t     rate;      // bytes / sec
        uint64_t     offset;    // msec
        uint64_t     duration;  // msec
    } __attribute__((packed));

    struct irelease_start_hdr {
        common_hdr   base;
        uint32_t     flow;      // flow ID
        uint64_t     rate;      // bytes / sec
        uint64_t     duration;  // msec
    } __attribute__((packed));

    struct irelease_stop_hdr {
        common_hdr   base;
        uint32_t     flow;
    } __attribute__((packed));
};

#endif
