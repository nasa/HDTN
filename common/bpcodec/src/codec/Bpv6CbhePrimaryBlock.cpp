/**
 * @file Bpv6CbhePrimaryBlock.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "codec/bpv6.h"
#include <inttypes.h>
#include "Sdnv.h"
#include <utility>
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

Bpv6CbhePrimaryBlock::Bpv6CbhePrimaryBlock() { } //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
Bpv6CbhePrimaryBlock::~Bpv6CbhePrimaryBlock() { } //a destructor: ~X()
Bpv6CbhePrimaryBlock::Bpv6CbhePrimaryBlock(const Bpv6CbhePrimaryBlock& o) :
    m_bundleProcessingControlFlags(o.m_bundleProcessingControlFlags),
    m_blockLength(o.m_blockLength),
    m_destinationEid(o.m_destinationEid),
    m_sourceNodeId(o.m_sourceNodeId),
    m_reportToEid(o.m_reportToEid),
    m_custodianEid(o.m_custodianEid),
    m_creationTimestamp(o.m_creationTimestamp),
    m_lifetimeSeconds(o.m_lifetimeSeconds),
    m_fragmentOffset(o.m_fragmentOffset),
    m_totalApplicationDataUnitLength(o.m_totalApplicationDataUnitLength) { } //a copy constructor: X(const X&)
Bpv6CbhePrimaryBlock::Bpv6CbhePrimaryBlock(Bpv6CbhePrimaryBlock&& o) :
    m_bundleProcessingControlFlags(o.m_bundleProcessingControlFlags),
    m_blockLength(o.m_blockLength),
    m_destinationEid(std::move(o.m_destinationEid)),
    m_sourceNodeId(std::move(o.m_sourceNodeId)),
    m_reportToEid(std::move(o.m_reportToEid)),
    m_custodianEid(std::move(o.m_custodianEid)),
    m_creationTimestamp(std::move(o.m_creationTimestamp)),
    m_lifetimeSeconds(o.m_lifetimeSeconds),
    m_fragmentOffset(o.m_fragmentOffset),
    m_totalApplicationDataUnitLength(o.m_totalApplicationDataUnitLength) { } //a move constructor: X(X&&)
Bpv6CbhePrimaryBlock& Bpv6CbhePrimaryBlock::operator=(const Bpv6CbhePrimaryBlock& o) { //a copy assignment: operator=(const X&)
    m_bundleProcessingControlFlags = o.m_bundleProcessingControlFlags;
    m_blockLength = o.m_blockLength;
    m_destinationEid = o.m_destinationEid;
    m_sourceNodeId = o.m_sourceNodeId;
    m_reportToEid = o.m_reportToEid;
    m_custodianEid = o.m_custodianEid;
    m_creationTimestamp = o.m_creationTimestamp;
    m_lifetimeSeconds = o.m_lifetimeSeconds;
    m_fragmentOffset = o.m_fragmentOffset;
    m_totalApplicationDataUnitLength = o.m_totalApplicationDataUnitLength;
    return *this;
}
Bpv6CbhePrimaryBlock& Bpv6CbhePrimaryBlock::operator=(Bpv6CbhePrimaryBlock && o) { //a move assignment: operator=(X&&)
    m_bundleProcessingControlFlags = o.m_bundleProcessingControlFlags;
    m_blockLength = o.m_blockLength;
    m_destinationEid = std::move(o.m_destinationEid);
    m_sourceNodeId = std::move(o.m_sourceNodeId);
    m_reportToEid = std::move(o.m_reportToEid);
    m_custodianEid = std::move(o.m_custodianEid);
    m_creationTimestamp = std::move(o.m_creationTimestamp);
    m_lifetimeSeconds = o.m_lifetimeSeconds;
    m_fragmentOffset = o.m_fragmentOffset;
    m_totalApplicationDataUnitLength = o.m_totalApplicationDataUnitLength;
    return *this;
}
bool Bpv6CbhePrimaryBlock::operator==(const Bpv6CbhePrimaryBlock & o) const {
    return (m_bundleProcessingControlFlags == o.m_bundleProcessingControlFlags)
        && (m_blockLength == o.m_blockLength)
        && (m_destinationEid == o.m_destinationEid)
        && (m_sourceNodeId == o.m_sourceNodeId)
        && (m_reportToEid == o.m_reportToEid)
        && (m_custodianEid == o.m_custodianEid)
        && (m_creationTimestamp == o.m_creationTimestamp)
        && (m_lifetimeSeconds == o.m_lifetimeSeconds)
        && (m_fragmentOffset == o.m_fragmentOffset)
        && (m_totalApplicationDataUnitLength == o.m_totalApplicationDataUnitLength);
}
bool Bpv6CbhePrimaryBlock::operator!=(const Bpv6CbhePrimaryBlock & o) const {
    return !(*this == o);
}
void Bpv6CbhePrimaryBlock::SetZero() {
    m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::NO_FLAGS_SET;
    m_blockLength = 0;
    m_destinationEid.SetZero();
    m_sourceNodeId.SetZero();
    m_reportToEid.SetZero();
    m_custodianEid.SetZero();
    m_creationTimestamp.SetZero();
    m_lifetimeSeconds = 0;
    m_fragmentOffset = 0;
    m_totalApplicationDataUnitLength = 0;
}

bool Bpv6CbhePrimaryBlock::DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    const uint8_t * const serializationBase = serialization;

    if (bufferSize == 0) { //version
        return false;
    }
    const uint8_t version = *serialization++;
    --bufferSize;
    if (version != BPV6_CCSDS_VERSION) {
        return false;
    }

    static constexpr unsigned int numSdnvsToDecode =
        2 + //flags, length
        8 + //dest, src, reportTo, custodian Eids
        2 + //creation timestamp
        1 + //lifetime
        1; //dictionary length expected 0
    uint64_t * const decodedSdnvs = (uint64_t *)(&m_bundleProcessingControlFlags); //start of the "array" instead of "uint64_t decodedSdnvs[numSdnvsToDecode];"
    uint64_t numBytesTakenToDecodeThisSdnvArray;
    bool decodeErrorDetected; //ignored because we expect all bytes to be present during decode, so it will be error free if return equals numSdnvsToDecode
    if (SdnvDecodeArrayU64(serialization, numBytesTakenToDecodeThisSdnvArray, decodedSdnvs, numSdnvsToDecode, bufferSize, decodeErrorDetected) != numSdnvsToDecode) {
        return false;
    }
    serialization += numBytesTakenToDecodeThisSdnvArray;
    //bufferSize -= numBytesTakenToDecodeThisSdnvArray; //do this in the if(isFragment)

    //the following copies are no longer needed as the SdnvDecodeArrayU64 will treat the members as an array starting at &m_bundleProcessingControlFlags
    //m_bundleProcessingControlFlags = static_cast<BPV6_BUNDLEFLAG>(decodedSdnvs[0]);
    //m_blockLength = decodedSdnvs[1];
    //m_destinationEid.Set(decodedSdnvs[2], decodedSdnvs[3]);
    //m_sourceNodeId.Set(decodedSdnvs[4], decodedSdnvs[5]);
    //m_reportToEid.Set(decodedSdnvs[6], decodedSdnvs[7]);
    //m_custodianEid.Set(decodedSdnvs[8], decodedSdnvs[9]);
    //m_creationTimestamp.Set(decodedSdnvs[10], decodedSdnvs[11]);
    //m_lifetimeSeconds = decodedSdnvs[12];
    //m_tmpDictionaryLengthIgnoredAndAssumedZero = decodedSdnvs[13];

    if (m_tmpDictionaryLengthIgnoredAndAssumedZero != 0) { //dictionary length (must be 1 byte zero value (1-byte sdnv's are the value itself) 
        //RFC6260
        //3.2.  Reception
        //
        //Upon receiving a bundle whose dictionary length is zero(and only in
        //this circumstance), a CBHE - conformant convergence - layer adapter :
        //
        //1.  MAY infer that the CLA from which the bundle was received is CBHE
        //    conformant.
        //
        //2.  MUST decode the primary block of the bundle in accordance with
        //    the CBHE compression convention described in Section 2.2 before
        //    delivering it to the bundle protocol agent.
        //
        //    Note that when a CLA that is not CBHE conformant receives a bundle
        //    whose dictionary length is zero, it has no choice but to pass it to
        //    the bundle agent without modification.In this case, the bundle
        //    protocol agent will be unable to dispatch the received bundle,
        //    because it will be unable to determine the destination endpoint; the
        //    bundle will be judged to be malformed.The behavior of the bundle
        //    protocol agent in this circumstance is an implementation matter.
        printf("error: cbhe bpv6 primary decode: dictionary size not 0\n");
        return false;
    }
    // Skip the entirety of the dictionary - we assume an IPN scheme

    const bool isFragment = ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::ISFRAGMENT) != BPV6_BUNDLEFLAG::NO_FLAGS_SET);
    if (isFragment) {
        bufferSize -= numBytesTakenToDecodeThisSdnvArray;
        uint64_t * const decodedSdnvsFragment = &m_fragmentOffset;
        if (SdnvDecodeArrayU64(serialization, numBytesTakenToDecodeThisSdnvArray, decodedSdnvsFragment, 2, bufferSize, decodeErrorDetected) != 2) {
            return false;
        }
        serialization += numBytesTakenToDecodeThisSdnvArray;
        //bufferSize -= numBytesTakenToDecodeThisSdnvArray; //not needed (not used past this point)

        //the following copies are no longer needed as the SdnvDecodeArrayU64 will treat the members as a two element array starting at &m_fragmentOffset
        //m_fragmentOffset = decodedSdnvs[0];
        //m_totalApplicationDataUnitLength = decodedSdnvs[1];
    }
    else {
        m_fragmentOffset = 0;
        m_totalApplicationDataUnitLength = 0;
    }

    numBytesTakenToDecode = serialization - serializationBase;
    return true;
}

uint64_t Bpv6CbhePrimaryBlock::SerializeBpv6(uint8_t * serialization) { //modifies m_blockLength
    uint8_t * const serializationBase = serialization;

    *serialization++ = BPV6_CCSDS_VERSION;
    serialization += SdnvEncodeU64BufSize10(serialization, static_cast<uint64_t>(m_bundleProcessingControlFlags));
    const bool isFragment = ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::ISFRAGMENT) != BPV6_BUNDLEFLAG::NO_FLAGS_SET);

    uint8_t * const blockLengthPtrForLater = serialization++; // we skip one byte so we can come back and write it later
    
    serialization += m_destinationEid.SerializeBpv6(serialization);
    serialization += m_sourceNodeId.SerializeBpv6(serialization);
    serialization += m_reportToEid.SerializeBpv6(serialization);
    serialization += m_custodianEid.SerializeBpv6(serialization);
    
    serialization += m_creationTimestamp.SerializeBpv6(serialization);

    serialization += SdnvEncodeU64BufSize10(serialization, m_lifetimeSeconds);
    
    // encode a zero-length dictionary
    *serialization++ = 0; // 1-byte sdnv's are the value itself

    if (isFragment) {
        serialization += SdnvEncodeU64BufSize10(serialization, m_fragmentOffset);
        serialization += SdnvEncodeU64BufSize10(serialization, m_totalApplicationDataUnitLength);
    }

    m_blockLength = serialization - (blockLengthPtrForLater + 1);
    if (m_blockLength > 127) { // our encoding failed because our block length was too long ...
        printf("error in Bpv6CbhePrimaryBlock::SerializeBpv6 blockLength > 127\n");
        return 0;
    }
    *blockLengthPtrForLater = static_cast<uint8_t>(m_blockLength); // 1-byte sdnv's are the value itself
    
    return serialization - serializationBase;
}

uint64_t Bpv6CbhePrimaryBlock::GetSerializationSize() const {
    const bool isFragment = ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::ISFRAGMENT) != BPV6_BUNDLEFLAG::NO_FLAGS_SET);

    uint64_t serializationSize = 3; //version6 + blockLength + zeroLengthDictionarySize
    const uint64_t sizeOfBundleProcessingControlFlags = SdnvGetNumBytesRequiredToEncode(static_cast<uint64_t>(m_bundleProcessingControlFlags));
    serializationSize += sizeOfBundleProcessingControlFlags;
    serializationSize += m_destinationEid.GetSerializationSizeBpv6();
    serializationSize += m_sourceNodeId.GetSerializationSizeBpv6();
    serializationSize += m_reportToEid.GetSerializationSizeBpv6();
    serializationSize += m_custodianEid.GetSerializationSizeBpv6();
    serializationSize += m_creationTimestamp.GetSerializationSizeBpv6();
    serializationSize += SdnvGetNumBytesRequiredToEncode(m_lifetimeSeconds);
    if (isFragment) {
        serializationSize += SdnvGetNumBytesRequiredToEncode(m_fragmentOffset);
        serializationSize += SdnvGetNumBytesRequiredToEncode(m_totalApplicationDataUnitLength);
    }
    const uint64_t blockLength = serializationSize - (sizeOfBundleProcessingControlFlags + 2); //+2 => version6 + blockLength
    if (blockLength > 127) { // our encoding failed because our block length was too long ...
        return 0;
    }
    return serializationSize;
}


void Bpv6CbhePrimaryBlock::bpv6_primary_block_print() const {
    printf("BPv6 / Primary block (%" PRIu64 " bytes)\n", m_blockLength);
    printf("Flags: 0x%" PRIx64 "\n", static_cast<uint64_t>(m_bundleProcessingControlFlags));
    if((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::NOFRAGMENT) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* No fragmentation allowed\n");
    }
    if ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::ISFRAGMENT) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* Bundle is a fragment\n");
    }
    if ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::ADMINRECORD) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* Bundle is administrative (control) traffic\n");
    }
    if ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::CUSTODY_REQUESTED) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* Custody transfer requested\n");
    }
    if ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::USER_APP_ACK_REQUESTED) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* Application acknowledgment requested.\n");
    }
    if ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::CUSTODY_STATUS_REPORTS_REQUESTED) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* Custody reporting requested.\n");
    }
    if ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::DELIVERY_STATUS_REPORTS_REQUESTED) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* Delivery reporting requested.\n");
    }
    if ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::DELETION_STATUS_REPORTS_REQUESTED) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* Deletion reporting requested.\n");
    }
    if ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::FORWARDING_STATUS_REPORTS_REQUESTED) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* Forward reporting requested.\n");
    }
    if ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::RECEPTION_STATUS_REPORTS_REQUESTED) != BPV6_BUNDLEFLAG::NO_FLAGS_SET) {
        printf("* Reception reporting requested.\n");
    }
    const BPV6_PRIORITY priority = GetPriorityFromFlags(m_bundleProcessingControlFlags);
    LOG_INFO(subprocess) << "Priority: " << priority;

    LOG_INFO(subprocess) << "Destination: " << m_destinationEid;
    LOG_INFO(subprocess) << "Source: " << m_sourceNodeId;
    LOG_INFO(subprocess) << "Custodian: " << m_custodianEid;
    LOG_INFO(subprocess) << "Report-to: " << m_reportToEid;

    LOG_INFO(subprocess) << "Creation: " << m_creationTimestamp;
    printf("Lifetime: %" PRIu64 "\n", m_lifetimeSeconds);
}


bool Bpv6CbhePrimaryBlock::HasCustodyFlagSet() const {
    return ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::CUSTODY_REQUESTED) != BPV6_BUNDLEFLAG::NO_FLAGS_SET);
}
bool Bpv6CbhePrimaryBlock::HasFragmentationFlagSet() const {
    return ((m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::ISFRAGMENT) != BPV6_BUNDLEFLAG::NO_FLAGS_SET);
}

cbhe_bundle_uuid_t Bpv6CbhePrimaryBlock::GetCbheBundleUuidFromPrimary() const {
    cbhe_bundle_uuid_t uuid;
    uuid.creationSeconds = m_creationTimestamp.secondsSinceStartOfYear2000;
    uuid.sequence = m_creationTimestamp.sequenceNumber;
    uuid.srcEid = m_sourceNodeId;
    uuid.fragmentOffset = m_fragmentOffset;
    uuid.dataLength = m_totalApplicationDataUnitLength;
    return uuid;
}
cbhe_bundle_uuid_nofragment_t Bpv6CbhePrimaryBlock::GetCbheBundleUuidNoFragmentFromPrimary() const {
    cbhe_bundle_uuid_nofragment_t uuid;
    uuid.creationSeconds = m_creationTimestamp.secondsSinceStartOfYear2000;
    uuid.sequence = m_creationTimestamp.sequenceNumber;
    uuid.srcEid = m_sourceNodeId;
    return uuid;
}

cbhe_eid_t Bpv6CbhePrimaryBlock::GetFinalDestinationEid() const {
    return m_destinationEid;
}
uint8_t Bpv6CbhePrimaryBlock::GetPriority() const {
    return static_cast<uint8_t>(GetPriorityFromFlags(m_bundleProcessingControlFlags));
}
uint64_t Bpv6CbhePrimaryBlock::GetExpirationSeconds() const {
    return m_creationTimestamp.secondsSinceStartOfYear2000 + m_lifetimeSeconds;
}
uint64_t Bpv6CbhePrimaryBlock::GetSequenceForSecondsScale() const {
    return m_creationTimestamp.sequenceNumber;
}
uint64_t Bpv6CbhePrimaryBlock::GetExpirationMilliseconds() const {
    return (m_creationTimestamp.secondsSinceStartOfYear2000 + m_lifetimeSeconds) * 1000;
}
uint64_t Bpv6CbhePrimaryBlock::GetSequenceForMillisecondsScale() const {
    return m_creationTimestamp.sequenceNumber;
}
