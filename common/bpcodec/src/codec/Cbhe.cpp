/**
 * @file Cbhe.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
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


//Each BP endpoint ID (EID) SHALL be represented as a CBOR array
//comprising two items.
//The first item of the array SHALL be the code number identifying the
//endpoint ID's URI scheme, as defined in the registry of URI scheme
//code numbers for Bundle Protocol.  Each URI scheme code number SHALL
//be represented as a CBOR unsigned integer.
//The second item of the array SHALL be the applicable CBOR
//representation of the scheme-specific part (SSP) of the EID, defined
//as noted in the references(s) for the URI scheme code number
//registry entry for the EID's URI scheme.
//   $eid /= [
//     uri-code: 2,
//     SSP: [
//       nodenum: uint,
//       servicenum: uint
//     ]
//   ]
//
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
    uint8_t * const serializationBase = serialization;
    *serialization++ = (4U << 5) | 2; //major type 4, additional information 2
    if (nodeId == 0) { //dtn URI scheme for dtn:none (serviceId is a "don't care" for version 7 purposes)
        *serialization++ = 1; //uri-code: 1
        *serialization = 0; //if SSP is "none", the SSP SHALL be represented as a CBOR unsigned integer with the value zero.
        return 3; //only 3 bytes to encode dtn:none
    }
    else { //encode as ipn URI scheme
        *serialization++ = 2; //uri-code: 2
        serialization += CborTwoUint64ArraySerialize(serialization, nodeId, serviceId);
        return serialization - serializationBase;
    }
}
uint64_t cbhe_eid_t::SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) const {
    uint8_t * const serializationBase = serialization;
    if (bufferSize < 3) { //initialCborByte + uriCodeByte + [sspCodeZeroForDtnScheme | atLeastOneByteForIpnScheme]
        return 0;
    }
    *serialization++ = (4U << 5) | 2; //major type 4, additional information 2
    if (nodeId == 0) { //dtn URI scheme for dtn:none (serviceId is a "don't care" for version 7 purposes)
        *serialization++ = 1; //uri-code: 1
        *serialization = 0; //if SSP is "none", the SSP SHALL be represented as a CBOR unsigned integer with the value zero.
        return 3; //only 3 bytes to encode dtn:none
    }
    else { //encode as ipn URI scheme
        bufferSize -= 2; //initialCborByte + uriCodeByte
        *serialization++ = 2; //uri-code: 2
        serialization += CborTwoUint64ArraySerialize(serialization, nodeId, serviceId, bufferSize);
        return serialization - serializationBase;
    }
}
uint64_t cbhe_eid_t::GetSerializationSizeBpv7() const {
    if (nodeId) { //ipn URI scheme
        return 2 + CborTwoUint64ArraySerializationSize(nodeId, serviceId); //2 => outer array byte + uri-code byte
    }
    return 3; //dtn URI scheme of dtn:none ... initialCborByte + uriCodeByte + sspCodeZeroForDtnScheme
}
bool cbhe_eid_t::DeserializeBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize) {
    //two element CBOR array: (uri code + SSP)
    // eid-structure = [
    //   uri-code: uint,
    //   SSP: any
    // ]
    // $eid /= [
    //   uri-code: 1,
    //   SSP: (tstr / 0)
    // ]
    // $eid /= [
    //   uri-code: 2,
    //   SSP: [
    //     nodenum: uint,
    //     servicenum: uint
    //   ]
    // ]
    const uint8_t * const serializationBase = serialization;
    if (bufferSize < 3) { //initialCborByte + uriCodeByte + [sspCodeZeroForDtnScheme | atLeastOneByteForIpnScheme]
        return false;
    }
    bufferSize -= 2; //initialCborByte + uriCodeByte
    const uint8_t initialCborByte = *serialization++;
    if ((initialCborByte != ((4U << 5) | 2U)) && //major type 4, additional information 2 (array of length 2)
        (initialCborByte != ((4U << 5) | 31U))) { //major type 4, additional information 31 (Indefinite-Length Array)
        return false;
    }
    const uint8_t uriCodeByte = *serialization++;
    if (uriCodeByte == 1) { //only support dtn:none for the DTN scheme
        //4.2.5.1.1.  The dtn URI Scheme
        //Encoding considerations:  For transmission as a BP endpoint ID, the
        //scheme-specific part of a URI of the dtn scheme SHALL be
        //represented as a CBOR text string unless the EID's SSP is "none",
        //in which case the SSP SHALL be represented as a CBOR unsigned
        //integer with the value zero.

        --bufferSize; //verified above with if (bufferSize < 3)
        const uint8_t dtnSchemeSspInitialByte = *serialization++;
        if (dtnSchemeSspInitialByte != 0) { //if unsupported dtn CBOR tstr
            return false;
        }
        //per https://datatracker.ietf.org/doc/rfc6260/ section 2.2, set both the node number and service number to 0 to represent dtn:none
        nodeId = 0;
        serviceId = 0;
    }
    else if (uriCodeByte == 2) { //ipn URI scheme
        uint8_t numBytesTakenToDecodeUri;
        if (!CborTwoUint64ArrayDeserialize(serialization, &numBytesTakenToDecodeUri, bufferSize, nodeId, serviceId)) {
            return false;
        }
        serialization += numBytesTakenToDecodeUri;
        bufferSize -= numBytesTakenToDecodeUri;
    }
    else { //uri code other (not supported)
        return false;
    }
    //An implementation of the Bundle Protocol MAY accept a sequence of
    //bytes that does not conform to the Bundle Protocol specification
    //(e.g., one that represents data elements in fixed-length arrays
    //rather than indefinite-length arrays) and transform it into
    //conformant BP structure before processing it.
    if (initialCborByte == ((4U << 5) | 31U)) { //major type 4, additional information 31 (Indefinite-Length Array)
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        if (breakStopCode != 0xff) {
            return false;
        }
    }

    *numBytesTakenToDecode = static_cast<uint8_t>(serialization - serializationBase);
    return true;
}


uint64_t cbhe_eid_t::SerializeBpv6(uint8_t * serialization) const {
    uint8_t * const serializationBase = serialization;
    serialization += SdnvEncodeU64BufSize10(serialization, nodeId);
    serialization += SdnvEncodeU64BufSize10(serialization, serviceId);
    return serialization - serializationBase;
}
uint64_t cbhe_eid_t::SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const {
    uint8_t * serializationBase = serialization;
    uint64_t thisSerializationSize;

    thisSerializationSize = SdnvEncodeU64(serialization, nodeId, bufferSize); //if zero returned on failure will only malform the bundle but not cause memory overflow
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    thisSerializationSize = SdnvEncodeU64(serialization, serviceId, bufferSize);
    serialization += thisSerializationSize;
    //bufferSize -= thisSerializationSize; //not needed

    return serialization - serializationBase;
}
uint64_t cbhe_eid_t::GetSerializationSizeBpv6() const {
    return SdnvGetNumBytesRequiredToEncode(nodeId) + SdnvGetNumBytesRequiredToEncode(serviceId);
}
bool cbhe_eid_t::DeserializeBpv6(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;

    nodeId = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    serviceId = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
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

