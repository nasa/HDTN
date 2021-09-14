#include <string.h>
#include <iostream>
#include "BpGenAsync.h"

struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};

BpGenAsync::BpGenAsync(uint64_t bundleSizeBytes) :
    BpSourcePattern(),
    m_bundleSizeBytes(bundleSizeBytes),
    m_bpGenSequenceNumber(0)
{

}

BpGenAsync::~BpGenAsync() {}


uint64_t BpGenAsync::GetNextPayloadLength_Step1() {
    return m_bundleSizeBytes;
}
bool BpGenAsync::CopyPayload_Step2(uint8_t * destinationBuffer) {
    bpgen_hdr bpGenHeader;
    bpGenHeader.seq = m_bpGenSequenceNumber++;
    memcpy(destinationBuffer, &bpGenHeader, sizeof(bpgen_hdr));
    return true;
}
