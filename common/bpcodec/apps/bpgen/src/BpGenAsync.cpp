/**
 * @file BpGenAsync.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

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
