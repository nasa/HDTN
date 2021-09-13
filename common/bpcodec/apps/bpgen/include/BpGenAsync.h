#ifndef _BPGEN_ASYNC_H
#define _BPGEN_ASYNC_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "OutductManager.h"
#include "InductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferEnhancementBlock.h"
#include "codec/CustodyTransferManager.h"
#include "app_patterns/BpSourcePattern.h"

class BpGenAsync : public BpSourcePattern {
private:
    BpGenAsync();
public:
    BpGenAsync(uint64_t bundleSizeBytes);
    virtual ~BpGenAsync();
    
protected:
    virtual uint64_t GetNextPayloadLength_Step1();
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer);
private:
    uint64_t m_bundleSizeBytes;
    uint64_t m_bpGenSequenceNumber;
};


#endif //_BPGEN_ASYNC_H
