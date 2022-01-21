/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "codec/Cbhe.h"
#include <utility>
#include "CborUint.h"
#include "Sdnv.h"

cbhe_eid_t::cbhe_eid_t() :
    nodeId(0),
    serviceId(0) { } //a default constructor: X()
cbhe_eid_t::cbhe_eid_t(uint64_t paramNodeId, uint64_t paramServiceId) :
    nodeId(paramNodeId),
    serviceId(paramServiceId) { }
cbhe_eid_t::~cbhe_eid_t() { } //a destructor: ~X()
cbhe_eid_t::cbhe_eid_t(const cbhe_eid_t& o) :
    nodeId(o.nodeId),
    serviceId(o.serviceId) { } //a copy constructor: X(const X&)
cbhe_eid_t::cbhe_eid_t(cbhe_eid_t&& o) :
    nodeId(o.nodeId),
    serviceId(o.serviceId) { } //a move constructor: X(X&&)
cbhe_eid_t& cbhe_eid_t::operator=(const cbhe_eid_t& o) { //a copy assignment: operator=(const X&)
    nodeId = o.nodeId;
    serviceId = o.serviceId;
    return *this;
}
cbhe_eid_t& cbhe_eid_t::operator=(cbhe_eid_t && o) { //a move assignment: operator=(X&&)
    nodeId = o.nodeId;
    serviceId = o.serviceId;
    return *this;
}
bool cbhe_eid_t::operator==(const cbhe_eid_t & o) const {
    return (nodeId == o.nodeId) && (serviceId == o.serviceId);
}
bool cbhe_eid_t::operator!=(const cbhe_eid_t & o) const {
    return (nodeId != o.nodeId) || (serviceId != o.serviceId);
}
bool cbhe_eid_t::operator<(const cbhe_eid_t & o) const {
    if (nodeId == o.nodeId) {
        return (serviceId < o.serviceId);
    }
    return (nodeId < o.nodeId);
}
void cbhe_eid_t::Set(uint64_t paramNodeId, uint64_t paramServiceId) {
    nodeId = paramNodeId;
    serviceId = paramServiceId;
}
void cbhe_eid_t::SetZero() {
    nodeId = 0;
    serviceId = 0;
}
std::ostream& operator<<(std::ostream& os, const cbhe_eid_t & o) {
    if ((o.nodeId == 0) && (o.serviceId == 0)) {
        os << "dtn:none";
    }
    else {
        os << "ipn: " << o.nodeId << "." << o.serviceId;
    }
    return os;
}

//For transmission as a BP endpoint ID, the
//scheme-specific part of a URI of the ipn scheme the SSP SHALL be
//represented as a CBOR array comprising two items.  The first item of
//this array SHALL be the EID's node number (a number that identifies
//the node) represented as a CBOR unsigned integer.  The second item
//of this array SHALL be the EID's service number (a number that
//identifies some application service) represented as a CBOR unsigned
//integer.  For all other purposes, URIs of the ipn scheme are encoded
//exclusively in US-ASCII characters.
uint64_t cbhe_eid_t::SerializeBpv7(uint8_t * serialization) const {
    return CborTwoUint64ArraySerialize(serialization, nodeId, serviceId);
}
uint64_t cbhe_eid_t::GetSerializationSizeBpv7() const {
    return CborTwoUint64ArraySerializationSize(nodeId, serviceId);
}
bool cbhe_eid_t::DeserializeBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize) {
    return CborTwoUint64ArrayDeserialize(serialization, numBytesTakenToDecode, bufferSize, nodeId, serviceId);
}


uint64_t cbhe_eid_t::SerializeBpv6(uint8_t * serialization) const {
    uint8_t * const serializationBase = serialization;
    serialization += SdnvEncodeU64(serialization, nodeId);
    serialization += SdnvEncodeU64(serialization, serviceId);
    return serialization - serializationBase;
}
uint64_t cbhe_eid_t::GetSerializationSizeBpv6() const {
    return SdnvGetNumBytesRequiredToEncode(nodeId) + SdnvGetNumBytesRequiredToEncode(serviceId);
}
bool cbhe_eid_t::DeserializeBpv6(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;

    if (bufferSize < SDNV_DECODE_MINIMUM_SAFE_BUFFER_SIZE) {
        return false;
    }
    nodeId = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (bufferSize < SDNV_DECODE_MINIMUM_SAFE_BUFFER_SIZE) {
        return false;
    }
    serviceId = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;

    *numBytesTakenToDecode = static_cast<uint8_t>(serialization - serializationBase);
    return true;
}


cbhe_bundle_uuid_t::cbhe_bundle_uuid_t() : 
    creationSeconds(0),
    sequence(0),
    fragmentOffset(0),
    dataLength(0) { } //a default constructor: X()
cbhe_bundle_uuid_t::cbhe_bundle_uuid_t(uint64_t paramCreationSeconds, uint64_t paramSequence,
    uint64_t paramSrcNodeId, uint64_t paramSrcServiceId, uint64_t paramFragmentOffset, uint64_t paramDataLength) :
    creationSeconds(paramCreationSeconds),
    sequence(paramSequence),
    srcEid(paramSrcNodeId, paramSrcServiceId),
    fragmentOffset(paramFragmentOffset),
    dataLength(paramDataLength) { }
cbhe_bundle_uuid_t::~cbhe_bundle_uuid_t() { } //a destructor: ~X()
cbhe_bundle_uuid_t::cbhe_bundle_uuid_t(const cbhe_bundle_uuid_t& o) : 
    creationSeconds(o.creationSeconds),
    sequence(o.sequence),
    srcEid(o.srcEid),
    fragmentOffset(o.fragmentOffset),
    dataLength(o.dataLength) { } //a copy constructor: X(const X&)
cbhe_bundle_uuid_t::cbhe_bundle_uuid_t(cbhe_bundle_uuid_t&& o) : 
    creationSeconds(o.creationSeconds),
    sequence(o.sequence),
    srcEid(std::move(o.srcEid)),
    fragmentOffset(o.fragmentOffset),
    dataLength(o.dataLength) { } //a move constructor: X(X&&)
cbhe_bundle_uuid_t& cbhe_bundle_uuid_t::operator=(const cbhe_bundle_uuid_t& o) { //a copy assignment: operator=(const X&)
    creationSeconds = o.creationSeconds;
    sequence = o.sequence;
    srcEid = o.srcEid;
    fragmentOffset = o.fragmentOffset;
    dataLength = o.dataLength;
    return *this;
}
cbhe_bundle_uuid_t& cbhe_bundle_uuid_t::operator=(cbhe_bundle_uuid_t && o) { //a move assignment: operator=(X&&)
    creationSeconds = o.creationSeconds;
    sequence = o.sequence;
    srcEid = std::move(o.srcEid);
    fragmentOffset = o.fragmentOffset;
    dataLength = o.dataLength;
    return *this;
}
bool cbhe_bundle_uuid_t::operator==(const cbhe_bundle_uuid_t & o) const {
    return 
        (creationSeconds == o.creationSeconds) &&
        (sequence == o.sequence) &&
        (srcEid == o.srcEid) &&
        (fragmentOffset == o.fragmentOffset) &&
        (dataLength == o.dataLength);
}
bool cbhe_bundle_uuid_t::operator!=(const cbhe_bundle_uuid_t & o) const {
    return 
        (creationSeconds != o.creationSeconds) ||
        (sequence != o.sequence) ||
        (srcEid != o.srcEid) ||
        (fragmentOffset != o.fragmentOffset) ||
        (dataLength != o.dataLength);
}
bool cbhe_bundle_uuid_t::operator<(const cbhe_bundle_uuid_t & o) const {
    if (creationSeconds == o.creationSeconds) {
        if (sequence == o.sequence) {
            if (srcEid == o.srcEid) {
                if (fragmentOffset == o.fragmentOffset) {
                    return (dataLength < o.dataLength);
                }
                return (fragmentOffset < o.fragmentOffset);
            }
            return (srcEid < o.srcEid);
        }
        return (sequence < o.sequence);
    }
    return (creationSeconds < o.creationSeconds);
}


cbhe_bundle_uuid_nofragment_t::cbhe_bundle_uuid_nofragment_t() :
    creationSeconds(0),
    sequence(0) { } //a default constructor: X()
cbhe_bundle_uuid_nofragment_t::cbhe_bundle_uuid_nofragment_t(uint64_t paramCreationSeconds, uint64_t paramSequence, uint64_t paramSrcNodeId, uint64_t paramSrcServiceId) :
    creationSeconds(paramCreationSeconds),
    sequence(paramSequence),
    srcEid(paramSrcNodeId, paramSrcServiceId) { }
cbhe_bundle_uuid_nofragment_t::cbhe_bundle_uuid_nofragment_t(const cbhe_bundle_uuid_t & bundleUuidWithFragment) :
    creationSeconds(bundleUuidWithFragment.creationSeconds),
    sequence(bundleUuidWithFragment.sequence),
    srcEid(bundleUuidWithFragment.srcEid) { }
cbhe_bundle_uuid_nofragment_t::~cbhe_bundle_uuid_nofragment_t() { } //a destructor: ~X()
cbhe_bundle_uuid_nofragment_t::cbhe_bundle_uuid_nofragment_t(const cbhe_bundle_uuid_nofragment_t& o) :
    creationSeconds(o.creationSeconds),
    sequence(o.sequence),
    srcEid(o.srcEid) { } //a copy constructor: X(const X&)
cbhe_bundle_uuid_nofragment_t::cbhe_bundle_uuid_nofragment_t(cbhe_bundle_uuid_nofragment_t&& o) :
    creationSeconds(o.creationSeconds),
    sequence(o.sequence),
    srcEid(std::move(o.srcEid)) { } //a move constructor: X(X&&)
cbhe_bundle_uuid_nofragment_t& cbhe_bundle_uuid_nofragment_t::operator=(const cbhe_bundle_uuid_nofragment_t& o) { //a copy assignment: operator=(const X&)
    creationSeconds = o.creationSeconds;
    sequence = o.sequence;
    srcEid = o.srcEid;
    return *this;
}
cbhe_bundle_uuid_nofragment_t& cbhe_bundle_uuid_nofragment_t::operator=(cbhe_bundle_uuid_nofragment_t && o) { //a move assignment: operator=(X&&)
    creationSeconds = o.creationSeconds;
    sequence = o.sequence;
    srcEid = std::move(o.srcEid);
    return *this;
}
bool cbhe_bundle_uuid_nofragment_t::operator==(const cbhe_bundle_uuid_nofragment_t & o) const {
    return
        (creationSeconds == o.creationSeconds) &&
        (sequence == o.sequence) &&
        (srcEid == o.srcEid);
}
bool cbhe_bundle_uuid_nofragment_t::operator!=(const cbhe_bundle_uuid_nofragment_t & o) const {
    return
        (creationSeconds != o.creationSeconds) ||
        (sequence != o.sequence) ||
        (srcEid != o.srcEid);
}
bool cbhe_bundle_uuid_nofragment_t::operator<(const cbhe_bundle_uuid_nofragment_t & o) const {
    if (creationSeconds == o.creationSeconds) {
        if (sequence == o.sequence) {
            return (srcEid < o.srcEid);
        }
        return (sequence < o.sequence);
    }
    return (creationSeconds < o.creationSeconds);
}

