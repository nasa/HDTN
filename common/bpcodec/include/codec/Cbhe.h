/**
 * 
 *
 * @author Brian Tomko
 * NOTICE: This code should be considered experimental, and should thus not be used in critical applications / contexts.
 */

#ifndef CBHE_H
#define CBHE_H

#include <cstdint>
#include <cstddef>
#include <ostream>
#include "bpcodec_export.h"

struct cbhe_eid_t {
    uint64_t nodeId;
    uint64_t serviceId;
    
    BPCODEC_EXPORT cbhe_eid_t(); //a default constructor: X()
    BPCODEC_EXPORT cbhe_eid_t(uint64_t paramNodeId, uint64_t paramServiceId);
    BPCODEC_EXPORT ~cbhe_eid_t(); //a destructor: ~X()
    BPCODEC_EXPORT cbhe_eid_t(const cbhe_eid_t& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT cbhe_eid_t(cbhe_eid_t&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT cbhe_eid_t& operator=(const cbhe_eid_t& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT cbhe_eid_t& operator=(cbhe_eid_t&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const cbhe_eid_t & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const cbhe_eid_t & o) const; //operator !=
    BPCODEC_EXPORT bool operator<(const cbhe_eid_t & o) const; //operator < so it can be used as a map key
    BPCODEC_EXPORT void Set(uint64_t paramNodeId, uint64_t paramServiceId);
    BPCODEC_EXPORT void SetZero();
    BPCODEC_EXPORT uint64_t SerializeBpv7(uint8_t * serialization) const;
    BPCODEC_EXPORT uint64_t SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) const;
    BPCODEC_EXPORT uint64_t GetSerializationSizeBpv7() const;
    BPCODEC_EXPORT bool DeserializeBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize);

    BPCODEC_EXPORT uint64_t SerializeBpv6(uint8_t * serialization) const;
    BPCODEC_EXPORT uint64_t SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const;
    BPCODEC_EXPORT uint64_t GetSerializationSizeBpv6() const;
    BPCODEC_EXPORT bool DeserializeBpv6(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize);

    BPCODEC_EXPORT friend std::ostream& operator<<(std::ostream& os, const cbhe_eid_t& o);
};

struct cbhe_bundle_uuid_t {

    //The creation timestamp is a pair of SDNVs that,
    //together with the source endpoint ID and (if the bundle is a
    //fragment) the fragment offset and payload length, serve to
    //identify the bundle.
    //A source Bundle Protocol Agent must never create two distinct bundles with the same source
    //endpoint ID and bundle creation timestamp.The combination of
    //source endpoint ID and bundle creation timestamp therefore serves
    //to identify a single transmission request, enabling it to be
    //acknowledged by the receiving application

    uint64_t creationSeconds;
    uint64_t sequence;
    cbhe_eid_t srcEid;
    //below if isFragment (default 0 if not a fragment)
    uint64_t fragmentOffset;
    uint64_t dataLength;      // 64 bytes



    BPCODEC_EXPORT cbhe_bundle_uuid_t(); //a default constructor: X()
    BPCODEC_EXPORT cbhe_bundle_uuid_t(uint64_t paramCreationSeconds, uint64_t paramSequence,
        uint64_t paramSrcNodeId, uint64_t paramSrcServiceId, uint64_t paramFragmentOffset, uint64_t paramDataLength);
    //cbhe_bundle_uuid_t(const Bpv6CbhePrimaryBlock & primary);
    BPCODEC_EXPORT ~cbhe_bundle_uuid_t(); //a destructor: ~X()
    BPCODEC_EXPORT cbhe_bundle_uuid_t(const cbhe_bundle_uuid_t& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT cbhe_bundle_uuid_t(cbhe_bundle_uuid_t&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT cbhe_bundle_uuid_t& operator=(const cbhe_bundle_uuid_t& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT cbhe_bundle_uuid_t& operator=(cbhe_bundle_uuid_t&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const cbhe_bundle_uuid_t & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const cbhe_bundle_uuid_t & o) const; //operator !=
    BPCODEC_EXPORT bool operator<(const cbhe_bundle_uuid_t & o) const; //operator < so it can be used as a map key
};

struct cbhe_bundle_uuid_nofragment_t {

    uint64_t creationSeconds;
    uint64_t sequence;
    cbhe_eid_t srcEid;

    BPCODEC_EXPORT cbhe_bundle_uuid_nofragment_t(); //a default constructor: X()
    BPCODEC_EXPORT cbhe_bundle_uuid_nofragment_t(uint64_t paramCreationSeconds, uint64_t paramSequence, uint64_t paramSrcNodeId, uint64_t paramSrcServiceId);
    //cbhe_bundle_uuid_nofragment_t(const Bpv6CbhePrimaryBlock & primary);
    BPCODEC_EXPORT cbhe_bundle_uuid_nofragment_t(const cbhe_bundle_uuid_t & bundleUuidWithFragment);
    BPCODEC_EXPORT ~cbhe_bundle_uuid_nofragment_t(); //a destructor: ~X()
    BPCODEC_EXPORT cbhe_bundle_uuid_nofragment_t(const cbhe_bundle_uuid_nofragment_t& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT cbhe_bundle_uuid_nofragment_t(cbhe_bundle_uuid_nofragment_t&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT cbhe_bundle_uuid_nofragment_t& operator=(const cbhe_bundle_uuid_nofragment_t& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT cbhe_bundle_uuid_nofragment_t& operator=(cbhe_bundle_uuid_nofragment_t&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const cbhe_bundle_uuid_nofragment_t & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const cbhe_bundle_uuid_nofragment_t & o) const; //operator !=
    BPCODEC_EXPORT bool operator<(const cbhe_bundle_uuid_nofragment_t & o) const; //operator < so it can be used as a map key
};


#endif //CBHE_H
