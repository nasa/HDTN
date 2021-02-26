#ifndef _HDTN_STATS_H
#define _HDTN_STATS_H

#include <stdint.h>

namespace hdtn {


struct FlowStats {
    uint64_t diskUsed;
    uint64_t diskWcount;
    uint64_t diskWbytes;
    uint64_t diskRcount;
    uint64_t diskRbytes;
};// __attribute__((packed));

struct WorkerStats {
    uint64_t imsgSent;
    uint64_t imsgReceived;
    FlowStats flow;
};// __attribute__((packed));

struct StorageFlowStats {
    StorageFlowStats() : src(0), dst(0), rate(0), duration(0), start(0) {}
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
};// __attribute__((packed));


struct StorageStats {
    StorageStats() : inMsg(0), inBytes(0), outMsg(0), outBytes(0), bytesUsed(0), bytesAvailable(0), rate(0) {}

    /**
     * Time at which stats were sent.  Only used during transmit - ignored otherwise
     */
    double ts;

    /**
     * Total message count in
     */
    uint64_t inMsg;

    /**
     * Total bytes in
     */
    uint64_t inBytes;

    /**
     * Total message count out
     */
    uint64_t outMsg;

    /**
     * Total bytes out
     */
    uint64_t outBytes;

    /**
     * Number of bytes used by storage
     */
    uint64_t bytesUsed;

    /**
     * Number of bytes available to storage
     */
    uint64_t bytesAvailable;

    /**
     * Rate at which data is presently being released from the system
     */
    uint64_t rate;

    /**
     * Contains information about worker thread and disk utilization
     */
    WorkerStats worker;
};// __attribute__((packed));
}  // namespace hdtn

#endif
