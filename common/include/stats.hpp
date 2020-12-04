#ifndef _HDTN_STATS_H
#define _HDTN_STATS_H

#include <stdint.h>

namespace hdtn {
    struct flow_stats {
        uint64_t disk_used;
        uint64_t disk_wcount;
        uint64_t disk_wbytes;
        uint64_t disk_rcount;
        uint64_t disk_rbytes;
    } __attribute__((packed));

    struct worker_stats {
        uint64_t imsg_sent;
        uint64_t imsg_received;
        flow_stats flow;
    } __attribute__((packed));

    struct storage_flow_stats {
        storage_flow_stats()
        : src(0), dst(0), rate(0), duration(0), start(0) { }
        /**
         * Source flow label
         */
        uint32_t src;
        /**
         * Destination flow label
         */
        uint32_t dst;
        /**
         * Maximum flow release rate
         */
        uint64_t rate;
        /**
         * Length of time release will last
         */
        uint64_t duration;
        /**
         * Time offset at which release is scheduled to begin
         */
        uint64_t start;
    } __attribute__((packed));

    struct storage_stats {
        storage_stats()
        : in_msg(0), in_bytes(0), out_msg(0), out_bytes(0), bytes_used(0), bytes_available(0), rate(0) { }
        /**
         * Time at which stats were sent.  Only used during transmit - ignored otherwise
         */
        double ts;

        /**
         * Total message count in
         */
        uint64_t in_msg;
        
        /**
         * Total bytes in
         */
        uint64_t in_bytes;
        
        /**
         * Total message count out
         */
        uint64_t out_msg;
        
        /**
         * Total bytes out
         */
        uint64_t out_bytes;
        
        /**
         * Number of bytes used by storage
         */
        uint64_t bytes_used;

        /**
         * Number of bytes available to storage
         */
        uint64_t bytes_available;

        /**
         * Rate at which data is presently being released from the system
         */
        uint64_t rate;

        /**
         * Contains information about worker thread and disk utilization
         */
        worker_stats worker;
    } __attribute__((packed));
}

#endif
