#ifndef _BP_SINK_ASYNC_H
#define _BP_SINK_ASYNC_H

#include <stdint.h>
#include "InductManager.h"
#include "OutductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferEnhancementBlock.h"
#include "codec/CustodyTransferManager.h"
#include <boost/asio.hpp>
#include "app_patterns/BpSinkPattern.h"

struct FinalStatsBpSink {
    FinalStatsBpSink() : m_totalBytesRx(0), m_totalBundlesRx(0), m_receivedCount(0), m_duplicateCount(0),
        m_seqHval(0), m_seqBase(0) {};
    uint64_t m_totalBytesRx;
    uint64_t m_totalBundlesRx;
    uint64_t m_receivedCount;
    uint64_t m_duplicateCount;
    uint64_t m_seqHval;
    uint64_t m_seqBase;
};

class BpSinkAsync : public BpSinkPattern {
public:
    BpSinkAsync();
    virtual ~BpSinkAsync();

protected:
    virtual bool ProcessPayload(const uint8_t * data, const uint64_t size);
public:

    FinalStatsBpSink m_FinalStatsBpSink;
};



#endif  //_BP_SINK_ASYNC_H
