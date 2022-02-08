/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */
#include "codec/bpv7.h"
#include "codec/Bpv7Crc.h"
#include "CborUint.h"
#include <boost/format.hpp>
#include <boost/make_unique.hpp>

////////////////////////////////////
// PREVIOUS NODE EXTENSION BLOCK
////////////////////////////////////

Bpv7PreviousNodeCanonicalBlock::Bpv7PreviousNodeCanonicalBlock() : Bpv7CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE;
} 
Bpv7PreviousNodeCanonicalBlock::~Bpv7PreviousNodeCanonicalBlock() { } //a destructor: ~X()
Bpv7PreviousNodeCanonicalBlock::Bpv7PreviousNodeCanonicalBlock(const Bpv7PreviousNodeCanonicalBlock& o) :
    Bpv7CanonicalBlock(o),
    m_previousNode(o.m_previousNode) { } //a copy constructor: X(const X&)
Bpv7PreviousNodeCanonicalBlock::Bpv7PreviousNodeCanonicalBlock(Bpv7PreviousNodeCanonicalBlock&& o) :
    Bpv7CanonicalBlock(std::move(o)),
    m_previousNode(std::move(o.m_previousNode)) { } //a move constructor: X(X&&)
Bpv7PreviousNodeCanonicalBlock& Bpv7PreviousNodeCanonicalBlock::operator=(const Bpv7PreviousNodeCanonicalBlock& o) { //a copy assignment: operator=(const X&)
    m_previousNode = o.m_previousNode;
    return static_cast<Bpv7PreviousNodeCanonicalBlock&>(Bpv7CanonicalBlock::operator=(o));
}
Bpv7PreviousNodeCanonicalBlock& Bpv7PreviousNodeCanonicalBlock::operator=(Bpv7PreviousNodeCanonicalBlock && o) { //a move assignment: operator=(X&&)
    m_previousNode = std::move(o.m_previousNode);
    return static_cast<Bpv7PreviousNodeCanonicalBlock&>(Bpv7CanonicalBlock::operator=(std::move(o)));
}
bool Bpv7PreviousNodeCanonicalBlock::operator==(const Bpv7PreviousNodeCanonicalBlock & o) const {
    return (m_previousNode == o.m_previousNode)
        && Bpv7CanonicalBlock::operator==(o);
}
bool Bpv7PreviousNodeCanonicalBlock::operator!=(const Bpv7PreviousNodeCanonicalBlock & o) const {
    return !(*this == o);
}
void Bpv7PreviousNodeCanonicalBlock::SetZero() {
    Bpv7CanonicalBlock::SetZero();
    m_previousNode.SetZero();
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE;
}

uint64_t Bpv7PreviousNodeCanonicalBlock::SerializeBpv7(uint8_t * serialization) {
    //The Previous Node block, block type 6, identifies the node that
    //forwarded this bundle to the local node (i.e., to the node at which
    //the bundle currently resides); its block-type-specific data is the
    //node ID of that forwarder node which SHALL take the form of a node
    //ID represented as described in Section 4.2.5.2. above.  If the local
    //node is the source of the bundle, then the bundle MUST NOT contain
    //any Previous Node block.  Otherwise the bundle SHOULD contain one
    //(1) occurrence of this type of block and MUST NOT contain more than
    //one.
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE;

    m_dataPtr = NULL;
    m_dataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv7CanonicalBlock::SerializeBpv7(serialization);
    uint64_t bufferSize = m_dataLength;
    uint8_t * blockSpecificDataSerialization = m_dataPtr;
    //uint64_t thisSerializationSize;


    //thisSerializationSize = 
    m_previousNode.SerializeBpv7(blockSpecificDataSerialization, bufferSize);
    //blockSpecificDataSerialization += thisSerializationSize;
    //bufferSize -= thisSerializationSize;

    RecomputeCrcAfterDataModification(serialization, serializationSizeCanonical);
    return serializationSizeCanonical;
}
uint64_t Bpv7PreviousNodeCanonicalBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return m_previousNode.GetSerializationSizeBpv7();
}

bool Bpv7PreviousNodeCanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv7() {
    uint8_t numBytesTakenToDecode;
    return ((m_dataPtr != NULL) && (m_previousNode.DeserializeBpv7(m_dataPtr, &numBytesTakenToDecode, m_dataLength)) && (numBytesTakenToDecode == m_dataLength));
}

////////////////////////////////////
// BUNDLE AGE EXTENSION BLOCK
////////////////////////////////////

Bpv7BundleAgeCanonicalBlock::Bpv7BundleAgeCanonicalBlock() : Bpv7CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE;
}
Bpv7BundleAgeCanonicalBlock::~Bpv7BundleAgeCanonicalBlock() { } //a destructor: ~X()
Bpv7BundleAgeCanonicalBlock::Bpv7BundleAgeCanonicalBlock(const Bpv7BundleAgeCanonicalBlock& o) :
    Bpv7CanonicalBlock(o),
    m_bundleAgeMilliseconds(o.m_bundleAgeMilliseconds) { } //a copy constructor: X(const X&)
Bpv7BundleAgeCanonicalBlock::Bpv7BundleAgeCanonicalBlock(Bpv7BundleAgeCanonicalBlock&& o) :
    Bpv7CanonicalBlock(std::move(o)),
    m_bundleAgeMilliseconds(o.m_bundleAgeMilliseconds) { } //a move constructor: X(X&&)
Bpv7BundleAgeCanonicalBlock& Bpv7BundleAgeCanonicalBlock::operator=(const Bpv7BundleAgeCanonicalBlock& o) { //a copy assignment: operator=(const X&)
    m_bundleAgeMilliseconds = o.m_bundleAgeMilliseconds;
    return static_cast<Bpv7BundleAgeCanonicalBlock&>(Bpv7CanonicalBlock::operator=(o));
}
Bpv7BundleAgeCanonicalBlock& Bpv7BundleAgeCanonicalBlock::operator=(Bpv7BundleAgeCanonicalBlock && o) { //a move assignment: operator=(X&&)
    m_bundleAgeMilliseconds = o.m_bundleAgeMilliseconds;
    return static_cast<Bpv7BundleAgeCanonicalBlock&>(Bpv7CanonicalBlock::operator=(std::move(o)));
}
bool Bpv7BundleAgeCanonicalBlock::operator==(const Bpv7BundleAgeCanonicalBlock & o) const {
    return (m_bundleAgeMilliseconds == o.m_bundleAgeMilliseconds)
        && Bpv7CanonicalBlock::operator==(o);
}
bool Bpv7BundleAgeCanonicalBlock::operator!=(const Bpv7BundleAgeCanonicalBlock & o) const {
    return !(*this == o);
}
void Bpv7BundleAgeCanonicalBlock::SetZero() {
    Bpv7CanonicalBlock::SetZero();
    m_bundleAgeMilliseconds = 0;
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE;
}

uint64_t Bpv7BundleAgeCanonicalBlock::SerializeBpv7(uint8_t * serialization) {
    //The Bundle Age block, block type 7, contains the number of
    //milliseconds that have elapsed between the time the bundle was
    //created and time at which it was most recently forwarded.  It is
    //intended for use by nodes lacking access to an accurate clock, to
    //aid in determining the time at which a bundle's lifetime expires.
    //The block-type-specific data of this block is an unsigned integer
    //containing the age of the bundle in milliseconds, which SHALL be
    //represented as a CBOR unsigned integer item. (The age of the bundle
    //is the sum of all known intervals of the bundle's residence at
    //forwarding nodes, up to the time at which the bundle was most
    //recently forwarded, plus the summation of signal propagation time
    //over all episodes of transmission between forwarding nodes.
    //Determination of these values is an implementation matter.) If the
    //bundle's creation time is zero, then the bundle MUST contain exactly
    //one (1) occurrence of this type of block; otherwise, the bundle MAY
    //contain at most one (1) occurrence of this type of block.  A bundle
    //MUST NOT contain multiple occurrences of the bundle age block, as
    //this could result in processing anomalies.
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE;
    m_dataPtr = NULL;
    m_dataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv7CanonicalBlock::SerializeBpv7(serialization);
    uint64_t bufferSize = m_dataLength;
    uint8_t * blockSpecificDataSerialization = m_dataPtr;
    //uint64_t thisSerializationSize;


    //thisSerializationSize = 
    CborEncodeU64(blockSpecificDataSerialization, m_bundleAgeMilliseconds, bufferSize);
    //blockSpecificDataSerialization += thisSerializationSize;
    //bufferSize -= thisSerializationSize;

    RecomputeCrcAfterDataModification(serialization, serializationSizeCanonical);
    return serializationSizeCanonical;
}
uint64_t Bpv7BundleAgeCanonicalBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return CborGetEncodingSizeU64(m_bundleAgeMilliseconds);
}

bool Bpv7BundleAgeCanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv7() {
    if (m_dataPtr == NULL) {
        return false;
    }
    uint8_t numBytesTakenToDecode;
    m_bundleAgeMilliseconds = CborDecodeU64(m_dataPtr, &numBytesTakenToDecode, m_dataLength);
    return ((numBytesTakenToDecode != 0) && (numBytesTakenToDecode == m_dataLength));
}

////////////////////////////////////
// HOP COUNT EXTENSION BLOCK
////////////////////////////////////

Bpv7HopCountCanonicalBlock::Bpv7HopCountCanonicalBlock() : Bpv7CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::HOP_COUNT;
}
Bpv7HopCountCanonicalBlock::~Bpv7HopCountCanonicalBlock() { } //a destructor: ~X()
Bpv7HopCountCanonicalBlock::Bpv7HopCountCanonicalBlock(const Bpv7HopCountCanonicalBlock& o) :
    Bpv7CanonicalBlock(o),
    m_hopLimit(o.m_hopLimit),
    m_hopCount(o.m_hopCount) { } //a copy constructor: X(const X&)
Bpv7HopCountCanonicalBlock::Bpv7HopCountCanonicalBlock(Bpv7HopCountCanonicalBlock&& o) :
    Bpv7CanonicalBlock(std::move(o)),
    m_hopLimit(o.m_hopLimit),
    m_hopCount(o.m_hopCount) { } //a move constructor: X(X&&)
Bpv7HopCountCanonicalBlock& Bpv7HopCountCanonicalBlock::operator=(const Bpv7HopCountCanonicalBlock& o) { //a copy assignment: operator=(const X&)
    m_hopLimit = o.m_hopLimit;
    m_hopCount = o.m_hopCount;
    return static_cast<Bpv7HopCountCanonicalBlock&>(Bpv7CanonicalBlock::operator=(o));
}
Bpv7HopCountCanonicalBlock& Bpv7HopCountCanonicalBlock::operator=(Bpv7HopCountCanonicalBlock && o) { //a move assignment: operator=(X&&)
    m_hopLimit = o.m_hopLimit;
    m_hopCount = o.m_hopCount;
    return static_cast<Bpv7HopCountCanonicalBlock&>(Bpv7CanonicalBlock::operator=(std::move(o)));
}
bool Bpv7HopCountCanonicalBlock::operator==(const Bpv7HopCountCanonicalBlock & o) const {
    return (m_hopLimit == o.m_hopLimit)
        && (m_hopCount == o.m_hopCount)
        && Bpv7CanonicalBlock::operator==(o);
}
bool Bpv7HopCountCanonicalBlock::operator!=(const Bpv7HopCountCanonicalBlock & o) const {
    return !(*this == o);
}
void Bpv7HopCountCanonicalBlock::SetZero() {
    Bpv7CanonicalBlock::SetZero();
    m_hopLimit = 0;
    m_hopCount = 0;
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::HOP_COUNT;
}

uint64_t Bpv7HopCountCanonicalBlock::SerializeBpv7(uint8_t * serialization) {
    //The Hop Count block, block type 10, contains two unsigned integers,
    //hop limit and hop count.  A "hop" is here defined as an occasion on
    //which a bundle was forwarded from one node to another node.  Hop
    //limit MUST be in the range 1 through 255. The hop limit value SHOULD
    //NOT be changed at any time after creation of the Hop Count block;
    //the hop count value SHOULD initially be zero and SHOULD be increased
    //by 1 on each hop.
    //
    //The hop count block is mainly intended as a safety mechanism, a
    //means of identifying bundles for removal from the network that can
    //never be delivered due to a persistent forwarding error.  Hop count
    //is particularly valuable as a defense against routing anomalies that
    //might cause a bundle to be forwarded in a cyclical "ping-pong"
    //fashion between two nodes.  When a bundle's hop count exceeds its
    //hop limit, the bundle SHOULD be deleted for the reason "hop limit
    //exceeded", following the bundle deletion procedure defined in
    //Section 5.10.
    //
    //Procedures for determining the appropriate hop limit for a bundle
    //are beyond the scope of this specification.
    //
    //The block-type-specific data in a hop count block SHALL be
    //represented as a CBOR array comprising two items.  The first item of
    //this array SHALL be the bundle's hop limit, represented as a CBOR
    //unsigned integer.  The second item of this array SHALL be the
    //bundle's hop count, represented as a CBOR unsigned integer. A bundle
    //MAY contain one occurrence of this type of block but MUST NOT
    //contain more than one.
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::HOP_COUNT;
    m_dataPtr = NULL;
    m_dataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv7CanonicalBlock::SerializeBpv7(serialization);
    uint64_t bufferSize = m_dataLength;
    uint8_t * blockSpecificDataSerialization = m_dataPtr;
    //uint64_t thisSerializationSize;


    //thisSerializationSize = 
    CborTwoUint64ArraySerialize(blockSpecificDataSerialization, m_hopLimit, m_hopCount, bufferSize);
    //blockSpecificDataSerialization += thisSerializationSize;
    //bufferSize -= thisSerializationSize;

    RecomputeCrcAfterDataModification(serialization, serializationSizeCanonical);
    return serializationSizeCanonical;
}
uint64_t Bpv7HopCountCanonicalBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return CborTwoUint64ArraySerializationSize(m_hopLimit, m_hopCount);
}

bool Bpv7HopCountCanonicalBlock::TryReserializeExtensionBlockDataWithoutResizeBpv7() {
    //If hop count doesn't transition from 23 to 24, and hop limit doesn't change, then
    //this block can be updated in place without data resize modifications.
    //If successful, call blocks[0]->headerPtr->RecomputeCrcAfterDataModification((uint8_t*)blocks[0]->actualSerializedBlockPtr.data(), blocks[0]->actualSerializedBlockPtr.size());
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::HOP_COUNT;
    uint8_t tempData[largestSerializedDataOnlySize];
    if (m_dataLength == CborTwoUint64ArraySerialize(tempData, m_hopLimit, m_hopCount)) {
        memcpy(m_dataPtr, tempData, m_dataLength);
        return true;
    }
    return false;
}

bool Bpv7HopCountCanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv7() {
    uint8_t numBytesTakenToDecode;
    return ((m_dataPtr != NULL) && (CborTwoUint64ArrayDeserialize(m_dataPtr, &numBytesTakenToDecode, m_dataLength, m_hopLimit, m_hopCount)) && (numBytesTakenToDecode == m_dataLength));
}


/////////////////////////////////////////
// ABSTRACT SECURITY (EXTENSION) BLOCK
/////////////////////////////////////////

Bpv7AbstractSecurityBlock::Bpv7AbstractSecurityBlock() : Bpv7CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    //m_blockTypeCode = ???;
}
Bpv7AbstractSecurityBlock::~Bpv7AbstractSecurityBlock() { } //a destructor: ~X()
Bpv7AbstractSecurityBlock::Bpv7AbstractSecurityBlock(Bpv7AbstractSecurityBlock&& o) :
    Bpv7CanonicalBlock(std::move(o)),
    m_securityTargets(std::move(o.m_securityTargets)),
    m_securityContextId(o.m_securityContextId),
    m_securityContextFlags(o.m_securityContextFlags),
    m_securitySource(std::move(o.m_securitySource)),
    m_securityContextParametersOptional(std::move(o.m_securityContextParametersOptional)),
    m_securityResults(std::move(o.m_securityResults)) { } //a move constructor: X(X&&)
Bpv7AbstractSecurityBlock& Bpv7AbstractSecurityBlock::operator=(Bpv7AbstractSecurityBlock && o) { //a move assignment: operator=(X&&)
    m_securityTargets = std::move(o.m_securityTargets);
    m_securityContextId = o.m_securityContextId;
    m_securityContextFlags = o.m_securityContextFlags;
    m_securitySource = std::move(o.m_securitySource);
    m_securityContextParametersOptional = std::move(o.m_securityContextParametersOptional);
    m_securityResults = std::move(o.m_securityResults);
    return static_cast<Bpv7AbstractSecurityBlock&>(Bpv7CanonicalBlock::operator=(std::move(o)));
}
bool Bpv7AbstractSecurityBlock::operator==(const Bpv7AbstractSecurityBlock & o) const {
    const bool initialTest = (m_securityTargets == o.m_securityTargets)
        && (m_securityContextId == o.m_securityContextId)
        && (m_securityContextFlags == o.m_securityContextFlags)
        && (m_securitySource == o.m_securitySource)
        //&& ((IsCipherSuiteParametersPresent()) ? (m_cipherSuiteParametersOptional == o.m_cipherSuiteParametersOptional) : true)
        //&& (m_securityResults == o.m_securityResults)
        && Bpv7CanonicalBlock::operator==(o);
    if (!initialTest) {
        return false;
    }
    if (IsSecurityContextParametersPresent()) {
        if (!Bpv7AbstractSecurityBlock::IsEqual(m_securityContextParametersOptional, o.m_securityContextParametersOptional)) {
            return false;
        }
    }
    return Bpv7AbstractSecurityBlock::IsEqual(m_securityResults, o.m_securityResults);
}
bool Bpv7AbstractSecurityBlock::operator!=(const Bpv7AbstractSecurityBlock & o) const {
    return !(*this == o);
}
void Bpv7AbstractSecurityBlock::SetZero() {
    Bpv7CanonicalBlock::SetZero();
    m_securityTargets.clear();
    m_securityContextId = 0;
    m_securityContextFlags = 0;
    m_securitySource.SetZero();
    m_securityContextParametersOptional.clear();
    m_securityResults.clear();
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PRIMARY_IMPLICIT_ZERO; //??
}


//Security Context Flags:
//This field identifies which optional fields are present in the
//security block.  This field SHALL be represented as a CBOR
//unsigned integer whose contents shall be interpreted as a bit
//field.  Each bit in this bit field indicates the presence (bit
//set to 1) or absence (bit set to 0) of optional data in the
//security block.  The association of bits to security block data
//is defined as follows.
//
//Bit 0  (the least-significant bit, 0x01): Security Context
//    Parameters Present Flag.
//
//Bit >0 Reserved
//
//Implementations MUST set reserved bits to 0 when writing this
//field and MUST ignore the values of reserved bits when reading
//this field.  For unreserved bits, a value of 1 indicates that
//the associated security block field MUST be included in the
//security block.  A value of 0 indicates that the associated
//security block field MUST NOT be in the security block.
bool Bpv7AbstractSecurityBlock::IsSecurityContextParametersPresent() const {
    return ((m_securityContextFlags & 0x1) != 0);
}
void Bpv7AbstractSecurityBlock::SetSecurityContextParametersPresent() {
    m_securityContextFlags |= 0x1;
}
void Bpv7AbstractSecurityBlock::ClearSecurityContextParametersPresent() {
    m_securityContextFlags &= ~(static_cast<uint8_t>(0x1));
}

uint64_t Bpv7AbstractSecurityBlock::SerializeBpv7(uint8_t * serialization) {
    //m_blockTypeCode = ???
    m_dataPtr = NULL;
    m_dataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv7CanonicalBlock::SerializeBpv7(serialization);
    uint64_t bufferSize = m_dataLength;
    uint8_t * blockSpecificDataSerialization = m_dataPtr;
    uint64_t thisSerializationSize;

    //The fields of the ASB SHALL be as follows, listed in the order in
    //which they must appear.  The encoding of these fields MUST be in
    //accordance with the canonical forms provided in Section 4.

    
    //Security Targets:
    //This field identifies the block(s) targeted by the security
    //operation(s) represented by this security block.  Each target
    //block is represented by its unique Block Number.  This field
    //SHALL be represented by a CBOR array of data items.  Each
    //target within this CBOR array SHALL be represented by a CBOR
    //unsigned integer.  This array MUST have at least 1 entry and
    //each entry MUST represent the Block Number of a block that
    //exists in the bundle.  There MUST NOT be duplicate entries in
    //this array.  The order of elements in this list has no semantic
    //meaning outside of the context of this block.  Within the
    //block, the ordering of targets must match the ordering of
    //results associated with these targets.
    thisSerializationSize = CborArbitrarySizeUint64ArraySerialize(blockSpecificDataSerialization, m_securityTargets, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;


    //Security Context Id:
    //This field identifies the security context used to implement
    //the security service represented by this block and applied to
    //each security target.  This field SHALL be represented by a
    //CBOR unsigned integer.  The values for this Id should come from
    //the registry defined in Section 11.3
    thisSerializationSize = CborEncodeU64(blockSpecificDataSerialization, m_securityContextId, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //Security Context Flags:
    //This field identifies which optional fields are present in the
    //security block.  This field SHALL be represented as a CBOR
    //unsigned integer whose contents shall be interpreted as a bit
    //field.  Each bit in this bit field indicates the presence (bit
    //set to 1) or absence (bit set to 0) of optional data in the
    //security block.  The association of bits to security block data
    //is defined as follows.
    //
    //Bit 0  (the least-significant bit, 0x01): Security Context
    //    Parameters Present Flag.
    //
    //Bit >0 Reserved
    //
    //Implementations MUST set reserved bits to 0 when writing this
    //field and MUST ignore the values of reserved bits when reading
    //this field.  For unreserved bits, a value of 1 indicates that
    //the associated security block field MUST be included in the
    //security block.  A value of 0 indicates that the associated
    //security block field MUST NOT be in the security block.
    thisSerializationSize = CborEncodeU64(blockSpecificDataSerialization, m_securityContextFlags, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //Security Source:
    //This field identifies the Endpoint that inserted the security
    //block in the bundle.  This field SHALL be represented by a CBOR
    //array in accordance with [I-D.ietf-dtn-bpbis] rules for
    //representing Endpoint Identifiers (EIDs).
    thisSerializationSize = m_securitySource.SerializeBpv7(blockSpecificDataSerialization, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //Security Context Parameters (Optional):
    //This field captures one or more security context parameters
    //that should be used when processing the security service
    //described by this security block.  This field SHALL be
    //represented by a CBOR array.  Each entry in this array is a
    //single security context parameter.  A single parameter SHALL
    //also be represented as a CBOR array comprising a 2-tuple of the
    //id and value of the parameter, as follows.
    //
    //*  Parameter Id.  This field identifies which parameter is
    //   being specified.  This field SHALL be represented as a CBOR
    //   unsigned integer.  Parameter Ids are selected as described
    //   in Section 3.10.
    //
    //*  Parameter Value.  This field captures the value associated
    //   with this parameter.  This field SHALL be represented by the
    //   applicable CBOR representation of the parameter, in
    //   accordance with Section 3.10.
    if (IsSecurityContextParametersPresent()) {
        thisSerializationSize = SerializeIdValuePairsVecBpv7(blockSpecificDataSerialization, m_securityContextParametersOptional, bufferSize);
        blockSpecificDataSerialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }

    //Security Results:
    //This field captures the results of applying a security service
    //to the security targets of the security block.  This field
    //SHALL be represented as a CBOR array of target results.  Each
    //entry in this array represents the set of security results for
    //a specific security target.  The target results MUST be ordered
    //identically to the Security Targets field of the security
    //block.  This means that the first set of target results in this
    //array corresponds to the first entry in the Security Targets
    //field of the security block, and so on.  There MUST be one
    //entry in this array for each entry in the Security Targets
    //field of the security block.
    //
    //The set of security results for a target is also represented as
    //a CBOR array of individual results.  An individual result is
    //represented as a 2-tuple of a result id and a result value,
    //defined as follows.
    //
    //*  Result Id.  This field identifies which security result is
    //   being specified.  Some security results capture the primary
    //   output of a cipher suite.  Other security results contain
    //   additional annotative information from cipher suite
    //   processing.  This field SHALL be represented as a CBOR
    //   unsigned integer.  Security result Ids will be as specified
    //   in Section 3.10.
    //
    //*  Result Value.  This field captures the value associated with
    //   the result.  This field SHALL be represented by the
    //   applicable CBOR representation of the result value, in
    //   accordance with Section 3.10.
    thisSerializationSize = SerializeIdValuePairsVecBpv7(blockSpecificDataSerialization, m_securityResults, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    
    RecomputeCrcAfterDataModification(serialization, serializationSizeCanonical);
    return serializationSizeCanonical;
}
uint64_t Bpv7AbstractSecurityBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    uint64_t serializationSize = CborArbitrarySizeUint64ArraySerializationSize(m_securityTargets);
    serializationSize += CborGetEncodingSizeU64(m_securityContextId);
    serializationSize += CborGetEncodingSizeU64(m_securityContextFlags);
    serializationSize += m_securitySource.GetSerializationSizeBpv7();
    if (IsSecurityContextParametersPresent()) {
        serializationSize += IdValuePairsVecBpv7SerializationSize(m_securityContextParametersOptional);
    }
    serializationSize += IdValuePairsVecBpv7SerializationSize(m_securityResults);
    return serializationSize;
}

bool Bpv7AbstractSecurityBlock::Virtual_DeserializeExtensionBlockDataBpv7() {
    static constexpr uint64_t maxElements = 1000; //todo
    uint8_t numBytesTakenToDecode;
    if (m_dataPtr == NULL) {
        return false;
    }

    uint8_t * serialization = m_dataPtr;
    uint64_t bufferSize = m_dataLength;
    uint8_t cborUintSizeDecoded;
    uint64_t tmpNumBytesTakenToDecode64;
    if (!CborArbitrarySizeUint64ArrayDeserialize(serialization, tmpNumBytesTakenToDecode64, bufferSize, m_securityTargets, maxElements)) {
        return false; //failure
    }
    serialization += tmpNumBytesTakenToDecode64;
    bufferSize -= tmpNumBytesTakenToDecode64;

    m_securityContextId = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
    if (cborUintSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborUintSizeDecoded;
    bufferSize -= cborUintSizeDecoded;

    const uint64_t tmpCipherSuiteFlags64 = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
    if (cborUintSizeDecoded == 0) {
        return false; //failure
    }
    if(tmpCipherSuiteFlags64 > 0x1f) {
        return false; //failure
    }
    m_securityContextFlags = static_cast<uint8_t>(tmpCipherSuiteFlags64);
    serialization += cborUintSizeDecoded;
    bufferSize -= cborUintSizeDecoded;

    if(!m_securitySource.DeserializeBpv7(serialization, &numBytesTakenToDecode, m_dataLength)) {
        return false; //failure
    }
    serialization += numBytesTakenToDecode;
    bufferSize -= numBytesTakenToDecode;

    if (IsSecurityContextParametersPresent()) {
        if(!DeserializeIdValuePairsVecBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize, m_securityContextParametersOptional,
            static_cast<BPSEC_SECURITY_CONTEXT_IDENTIFIERS>(m_securityContextId), true, maxElements))
        {
            return false; //failure
        }
        serialization += tmpNumBytesTakenToDecode64;
        bufferSize -= tmpNumBytesTakenToDecode64;
    }

    if (!DeserializeIdValuePairsVecBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize, m_securityResults,
        static_cast<BPSEC_SECURITY_CONTEXT_IDENTIFIERS>(m_securityContextId), false, maxElements))
    {
        return false; //failure
    }
    serialization += tmpNumBytesTakenToDecode64;
    bufferSize -= tmpNumBytesTakenToDecode64;

    return (bufferSize == 0);
}

//static helpers
//This field SHALL be represented by a CBOR array. Each entry in this array is also a CBOR array
//comprising a 2-tuple of the id and value, as follows.
//    Id. This field SHALL be represented as a CBOR unsigned integer.
//    Value. This field SHALL be represented by the applicable CBOR representation (treate as already raw cbor serialization here)
uint64_t Bpv7AbstractSecurityBlock::SerializeIdValuePairsVecBpv7(uint8_t * serialization, const id_value_pairs_vec_t & idValuePairsVec, uint64_t bufferSize) {
    uint8_t * const serializationBase = serialization;
    uint64_t thisSerializationSize;
    uint8_t * const arrayHeaderStartPtr = serialization;
    thisSerializationSize = CborEncodeU64(serialization, idValuePairsVec.size(), bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;
    *arrayHeaderStartPtr |= (4U << 5); //change from major type 0 (unsigned integer) to major type 4 (array)

    for (std::size_t i = 0; i < idValuePairsVec.size(); ++i) {
        const id_value_pair_t & idValuePair = idValuePairsVec[i];
        if (bufferSize == 0) {
            return 0;
        }
        *serialization++ = (4U << 5) | 2; //major type 4, additional information 2 (add cbor array of size 2)
        --bufferSize;

        thisSerializationSize = CborEncodeU64(serialization, idValuePair.first, bufferSize); //id
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;

        thisSerializationSize = idValuePair.second->SerializeBpv7(serialization, bufferSize); //value
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    return serialization - serializationBase;
}
uint64_t Bpv7AbstractSecurityBlock::IdValuePairsVecBpv7SerializationSize(const id_value_pairs_vec_t & idValuePairsVec) {
    uint64_t serializationSize = CborGetEncodingSizeU64(idValuePairsVec.size());
    serializationSize += idValuePairsVec.size(); //for major type 4, additional information 2 (cbor array of size 2) headers within the following loop
    for (std::size_t i = 0; i < idValuePairsVec.size(); ++i) {
        const id_value_pair_t & idValuePair = idValuePairsVec[i];
        serializationSize += CborGetEncodingSizeU64(idValuePair.first);
        serializationSize += idValuePair.second->GetSerializationSize(); //value
    }
    return serializationSize;
}
bool Bpv7AbstractSecurityBlock::DeserializeIdValuePairsVecBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize,
    id_value_pairs_vec_t & idValuePairsVec, const BPSEC_SECURITY_CONTEXT_IDENTIFIERS securityContext, const bool isForSecurityParameters, const uint64_t maxElements) {
    const uint8_t * const serializationBase = serialization;

    if (bufferSize == 0) {
        return false;
    }
    const uint8_t initialCborByte = *serialization; //buffer size verified above
    if (initialCborByte == ((4U << 5) | 31U)) { //major type 4, additional information 31 (Indefinite-Length Array)
        //An implementation of the Bundle Protocol MAY accept a sequence of
        //bytes that does not conform to the Bundle Protocol specification
        //(e.g., one that represents data elements in fixed-length arrays
        //rather than indefinite-length arrays) and transform it into
        //conformant BP structure before processing it.
        ++serialization;
        --bufferSize;
        idValuePairsVec.resize(0);
        idValuePairsVec.reserve(maxElements);
        for (std::size_t i = 0; i < maxElements; ++i) {
            idValuePairsVec.emplace_back();
            id_value_pair_t & idValuePair = idValuePairsVec.back();
            uint64_t tmpNumBytesTakenToDecode64;
            if (!DeserializeIdValuePairBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize, idValuePair, securityContext, isForSecurityParameters)) {
                return false; //failure
            }
            serialization += tmpNumBytesTakenToDecode64;
            bufferSize -= tmpNumBytesTakenToDecode64;
        }
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        if (breakStopCode != 0xff) {
            return false;
        }
    }
    else {
        uint8_t * const arrayHeaderStartPtr = serialization; //buffer size verified above
        const uint8_t cborMajorTypeArray = (*arrayHeaderStartPtr) >> 5;
        if (cborMajorTypeArray != 4) {
            return false; //failure
        }
        *arrayHeaderStartPtr &= 0x1f; //temporarily zero out major type to 0 to make it unsigned integer
        uint8_t cborUintSizeDecoded;
        const uint64_t numElements = CborDecodeU64(arrayHeaderStartPtr, &cborUintSizeDecoded, bufferSize);
        *arrayHeaderStartPtr |= (4U << 5); // restore to major type to 4 (change from major type 0 (unsigned integer) to major type 4 (array))
        if (cborUintSizeDecoded == 0) {
            return false; //failure
        }
        if (numElements > maxElements) {
            return false; //failure
        }
        serialization += cborUintSizeDecoded;
        bufferSize -= cborUintSizeDecoded;

        idValuePairsVec.resize(numElements);
        for (std::size_t i = 0; i < numElements; ++i) {
            id_value_pair_t & idValuePair = idValuePairsVec[i];
            uint64_t tmpNumBytesTakenToDecode64;
            if (!DeserializeIdValuePairBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize, idValuePair, securityContext, isForSecurityParameters)) {
                return false; //failure
            }
            serialization += tmpNumBytesTakenToDecode64;
            bufferSize -= tmpNumBytesTakenToDecode64;
        }
    }

    numBytesTakenToDecode = (serialization - serializationBase);
    return true;
}

bool Bpv7AbstractSecurityBlock::DeserializeIdValuePairBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize,
    id_value_pair_t & idValuePair, const BPSEC_SECURITY_CONTEXT_IDENTIFIERS securityContext, const bool isForSecurityParameters) {
    uint8_t cborUintSize;
    const uint8_t * const serializationBase = serialization;

    if (bufferSize == 0) {
        return false;
    }
    --bufferSize;
    const uint8_t initialCborByte = *serialization++;
    if ((initialCborByte != ((4U << 5) | 2U)) && //major type 4, additional information 2 (array of length 2)
        (initialCborByte != ((4U << 5) | 31U))) { //major type 4, additional information 31 (Indefinite-Length Array)
        return false;
    }

    idValuePair.first = CborDecodeU64(serialization, &cborUintSize, bufferSize);
    if (cborUintSize == 0) {
        return false; //failure
    }
    serialization += cborUintSize;
    bufferSize -= cborUintSize;

    uint64_t tmpNumBytesTakenToDecode64;
    if (isForSecurityParameters) {
        switch (securityContext) {
            case BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2:
                switch (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(idValuePair.first)) {
                    case BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::SHA_VARIANT:
                    case BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::INTEGRITY_SCOPE_FLAGS:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
                        break;
                    case BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::WRAPPED_KEY:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
                        break;
                    default:
                        return false;
                }
                break;
            case BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM:
                switch (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(idValuePair.first)) {
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AES_VARIANT:
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AAD_SCOPE_FLAGS:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
                        break;
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::INITIALIZATION_VECTOR:
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::WRAPPED_KEY:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
                        break;
                    default:
                        return false;
                }
                break;
            default:
                return false;
        }
    }
    else {
        switch (securityContext) {
            case BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2:
                switch (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_RESULTS>(idValuePair.first)) {
                    case BPSEC_BIB_HMAX_SHA2_SECURITY_RESULTS::EXPECTED_HMAC:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
                        break;
                    default:
                        return false;
                }
                break;
            case BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM:
                switch (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_RESULTS>(idValuePair.first)) {
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_RESULTS::AUTHENTICATION_TAG:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
                        break;
                    default:
                        return false;
                }
                break;
            default:
                return false;
        }
    }
    if(!idValuePair.second->DeserializeBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize)) {
        return false; //failure
    }
    serialization += tmpNumBytesTakenToDecode64;
    

    //An implementation of the Bundle Protocol MAY accept a sequence of
    //bytes that does not conform to the Bundle Protocol specification
    //(e.g., one that represents data elements in fixed-length arrays
    //rather than indefinite-length arrays) and transform it into
    //conformant BP structure before processing it.
    if (initialCborByte == ((4U << 5) | 31U)) { //major type 4, additional information 31 (Indefinite-Length Array)
        bufferSize -= tmpNumBytesTakenToDecode64; //from value above
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        if (breakStopCode != 0xff) {
            return false;
        }
    }

    numBytesTakenToDecode = static_cast<uint8_t>(serialization - serializationBase);
    return true;
}
bool Bpv7AbstractSecurityBlock::IsEqual(const id_value_pairs_vec_t & pVec1, const id_value_pairs_vec_t & pVec2) {
    if (pVec1.size() != pVec2.size()) {
        return false;
    }
    for (std::size_t i = 0; i < pVec1.size(); ++i) {
        const id_value_pair_t & p1 = pVec1[i];
        const id_value_pair_t & p2 = pVec2[i];
        if (p1.first != p2.first) {
            return false;
        }
        if ((p1.second) && (p2.second)) { //both not null
            if (!p1.second->IsEqual(p2.second.get())) {
                return false;
            }
        }
        else if ((!p1.second) && (!p2.second)) { //both null
            continue;
        }
        else {
            return false;
        }
    }
    return true;
}
void Bpv7AbstractSecurityBlock::SetSecurityContextId(BPSEC_SECURITY_CONTEXT_IDENTIFIERS id) {
    m_securityContextId = static_cast<security_context_id_t>(id);
}
std::vector<uint8_t> * Bpv7AbstractSecurityBlock::Protected_AppendAndGetSecurityResultByteStringPtr(uint64_t resultType) {
    //add it
    m_securityResults.emplace_back();
    m_securityResults.back().first = resultType;
    std::unique_ptr<Bpv7AbstractSecurityBlockValueByteString> v = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
    std::vector<uint8_t> * retVal = &(v->m_byteString);
    m_securityResults.back().second = std::move(v);
    return retVal;
}
std::vector<std::vector<uint8_t>*> Bpv7AbstractSecurityBlock::Protected_GetAllSecurityResultsByteStringPtrs(uint64_t resultType) {
    std::vector<std::vector<uint8_t>*> ptrs;
    for (std::size_t i = 0; i < m_securityResults.size(); ++i) {
        security_result_t & res = m_securityResults[i];
        if (res.first == resultType) {
            if (Bpv7AbstractSecurityBlockValueByteString * valueByteString = dynamic_cast<Bpv7AbstractSecurityBlockValueByteString*>(res.second.get())) {
                ptrs.push_back(&(valueByteString->m_byteString));
            }
        }
    }
    return ptrs;
}

/////////////////////////////////////////
// BLOCK INTEGRITY BLOCK
/////////////////////////////////////////

Bpv7BlockIntegrityBlock::Bpv7BlockIntegrityBlock() : Bpv7AbstractSecurityBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::INTEGRITY;
    SetSecurityContextId(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2);
}
Bpv7BlockIntegrityBlock::~Bpv7BlockIntegrityBlock() { } //a destructor: ~X()
Bpv7BlockIntegrityBlock::Bpv7BlockIntegrityBlock(Bpv7BlockIntegrityBlock&& o) :
    Bpv7AbstractSecurityBlock(std::move(o)) { } //a move constructor: X(X&&)
Bpv7BlockIntegrityBlock& Bpv7BlockIntegrityBlock::operator=(Bpv7BlockIntegrityBlock && o) { //a move assignment: operator=(X&&)
    return static_cast<Bpv7BlockIntegrityBlock&>(Bpv7AbstractSecurityBlock::operator=(std::move(o)));
}
bool Bpv7BlockIntegrityBlock::operator==(const Bpv7BlockIntegrityBlock & o) const {
    return Bpv7AbstractSecurityBlock::operator==(o);
}
bool Bpv7BlockIntegrityBlock::operator!=(const Bpv7BlockIntegrityBlock & o) const {
    return !(*this == o);
}
void Bpv7BlockIntegrityBlock::SetZero() {
    Bpv7AbstractSecurityBlock::SetZero();
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::INTEGRITY;
}
bool Bpv7BlockIntegrityBlock::AddOrUpdateSecurityParameterShaVariant(COSE_ALGORITHMS alg) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::SHA_VARIANT) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                valueUint->m_uintValue = static_cast<uint64_t>(alg);
                return true;
            }
            else {
                return false;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::SHA_VARIANT);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueUint> v = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
    v->m_uintValue = static_cast<uint64_t>(alg);
    m_securityContextParametersOptional.back().second = std::move(v);
    return true;
}
COSE_ALGORITHMS Bpv7BlockIntegrityBlock::GetSecurityParameterShaVariant(bool & success) const {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        const security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::SHA_VARIANT) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                success = true;
                return static_cast<COSE_ALGORITHMS>(valueUint->m_uintValue);
            }
            else {
                success = false;
                return static_cast<COSE_ALGORITHMS>(0);
            }
        }
    }
    success = false;
    return static_cast<COSE_ALGORITHMS>(0);
}
bool Bpv7BlockIntegrityBlock::AddSecurityParameterIntegrityScope(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS integrityScope) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::INTEGRITY_SCOPE_FLAGS) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                valueUint->m_uintValue |= static_cast<uint64_t>(integrityScope);
                return true;
            }
            else {
                return false;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::INTEGRITY_SCOPE_FLAGS);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueUint> v = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
    v->m_uintValue = static_cast<uint64_t>(integrityScope);
    m_securityContextParametersOptional.back().second = std::move(v);
    return true;
}
bool Bpv7BlockIntegrityBlock::IsSecurityParameterIntegrityScopePresentAndSet(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS integrityScope) const {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        const security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::INTEGRITY_SCOPE_FLAGS) {
            if (const Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<const Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                return (valueUint->m_uintValue & static_cast<uint64_t>(integrityScope)) == static_cast<uint64_t>(integrityScope);
            }
            else {
                return false;
            }
        }
    }
    return false;
}
std::vector<uint8_t> * Bpv7BlockIntegrityBlock::AddAndGetWrappedKeyPtr() {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::WRAPPED_KEY) {
            if (Bpv7AbstractSecurityBlockValueByteString * valueByteString = dynamic_cast<Bpv7AbstractSecurityBlockValueByteString*>(param.second.get())) {
                return &(valueByteString->m_byteString);
            }
            else {
                return NULL;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::WRAPPED_KEY);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueByteString> v = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
    std::vector<uint8_t> * retVal = &(v->m_byteString);
    m_securityContextParametersOptional.back().second = std::move(v);
    return retVal;
}
std::vector<uint8_t> * Bpv7BlockIntegrityBlock::AppendAndGetExpectedHmacPtr() {
    return Protected_AppendAndGetSecurityResultByteStringPtr(static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_RESULTS::EXPECTED_HMAC));
}
std::vector<std::vector<uint8_t>*> Bpv7BlockIntegrityBlock::GetAllExpectedHmacPtrs() {
    return Protected_GetAllSecurityResultsByteStringPtrs(static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_RESULTS::EXPECTED_HMAC));
}

/////////////////////////////////////////
// BLOCK CONFIDENTIALITY BLOCK
/////////////////////////////////////////

Bpv7BlockConfidentialityBlock::Bpv7BlockConfidentialityBlock() : Bpv7AbstractSecurityBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY;
    SetSecurityContextId(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM);
}
Bpv7BlockConfidentialityBlock::~Bpv7BlockConfidentialityBlock() { } //a destructor: ~X()
Bpv7BlockConfidentialityBlock::Bpv7BlockConfidentialityBlock(Bpv7BlockConfidentialityBlock&& o) :
    Bpv7AbstractSecurityBlock(std::move(o)) { } //a move constructor: X(X&&)
Bpv7BlockConfidentialityBlock& Bpv7BlockConfidentialityBlock::operator=(Bpv7BlockConfidentialityBlock && o) { //a move assignment: operator=(X&&)
    return static_cast<Bpv7BlockConfidentialityBlock&>(Bpv7AbstractSecurityBlock::operator=(std::move(o)));
}
bool Bpv7BlockConfidentialityBlock::operator==(const Bpv7BlockConfidentialityBlock & o) const {
    return Bpv7AbstractSecurityBlock::operator==(o);
}
bool Bpv7BlockConfidentialityBlock::operator!=(const Bpv7BlockConfidentialityBlock & o) const {
    return !(*this == o);
}
void Bpv7BlockConfidentialityBlock::SetZero() {
    Bpv7AbstractSecurityBlock::SetZero();
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY;
}

bool Bpv7BlockConfidentialityBlock::AddOrUpdateSecurityParameterAesVariant(COSE_ALGORITHMS alg) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AES_VARIANT) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                valueUint->m_uintValue = static_cast<uint64_t>(alg);
                return true;
            }
            else {
                return false;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AES_VARIANT);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueUint> v = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
    v->m_uintValue = static_cast<uint64_t>(alg);
    m_securityContextParametersOptional.back().second = std::move(v);
    return true;
}
COSE_ALGORITHMS Bpv7BlockConfidentialityBlock::GetSecurityParameterAesVariant(bool & success) const {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        const security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AES_VARIANT) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                success = true;
                return static_cast<COSE_ALGORITHMS>(valueUint->m_uintValue);
            }
            else {
                success = false;
                return static_cast<COSE_ALGORITHMS>(0);
            }
        }
    }
    success = false;
    return static_cast<COSE_ALGORITHMS>(0);
}
bool Bpv7BlockConfidentialityBlock::AddSecurityParameterScope(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS scope) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AAD_SCOPE_FLAGS) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                valueUint->m_uintValue |= static_cast<uint64_t>(scope);
                return true;
            }
            else {
                return false;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AAD_SCOPE_FLAGS);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueUint> v = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
    v->m_uintValue = static_cast<uint64_t>(scope);
    m_securityContextParametersOptional.back().second = std::move(v);
    return true;
}
bool Bpv7BlockConfidentialityBlock::IsSecurityParameterScopePresentAndSet(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS scope) const {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        const security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AAD_SCOPE_FLAGS) {
            if (const Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<const Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                return (valueUint->m_uintValue & static_cast<uint64_t>(scope)) == static_cast<uint64_t>(scope);
            }
            else {
                return false;
            }
        }
    }
    return false;
}
std::vector<uint8_t> * Bpv7BlockConfidentialityBlock::AddAndGetAesWrappedKeyPtr() {
    return Private_AddAndGetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::WRAPPED_KEY);
}
std::vector<uint8_t> * Bpv7BlockConfidentialityBlock::Private_AddAndGetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS parameter) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == parameter) {
            if (Bpv7AbstractSecurityBlockValueByteString * valueByteString = dynamic_cast<Bpv7AbstractSecurityBlockValueByteString*>(param.second.get())) {
                return &(valueByteString->m_byteString);
            }
            else {
                return NULL;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(parameter);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueByteString> v = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
    std::vector<uint8_t> * retVal = &(v->m_byteString);
    m_securityContextParametersOptional.back().second = std::move(v);
    return retVal;
}
std::vector<uint8_t> * Bpv7BlockConfidentialityBlock::AddAndGetInitializationVectorPtr() {
    return Private_AddAndGetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::INITIALIZATION_VECTOR);
}
std::vector<uint8_t> * Bpv7BlockConfidentialityBlock::AppendAndGetPayloadAuthenticationTagPtr() {
    return Protected_AppendAndGetSecurityResultByteStringPtr(static_cast<uint64_t>(BPSEC_BCB_AES_GCM_AAD_SECURITY_RESULTS::AUTHENTICATION_TAG));
}
std::vector<std::vector<uint8_t>*> Bpv7BlockConfidentialityBlock::GetAllPayloadAuthenticationTagPtrs() {
    return Protected_GetAllSecurityResultsByteStringPtrs(static_cast<uint64_t>(BPSEC_BCB_AES_GCM_AAD_SECURITY_RESULTS::AUTHENTICATION_TAG));
}


/////////////////////////////////////////
// VALUES FOR ABSTRACT SECURITY BLOCK
/////////////////////////////////////////
Bpv7AbstractSecurityBlockValueBase::~Bpv7AbstractSecurityBlockValueBase() {}

Bpv7AbstractSecurityBlockValueUint::~Bpv7AbstractSecurityBlockValueUint() {}
uint64_t Bpv7AbstractSecurityBlockValueUint::SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) {
    return CborEncodeU64(serialization, m_uintValue, bufferSize);
}
uint64_t Bpv7AbstractSecurityBlockValueUint::GetSerializationSize() const {
    return CborGetEncodingSizeU64(m_uintValue);
}
bool Bpv7AbstractSecurityBlockValueUint::DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t cborUintSize;
    m_uintValue = CborDecodeU64(serialization, &cborUintSize, bufferSize);
    numBytesTakenToDecode = cborUintSize;
    return (numBytesTakenToDecode != 0);
}
bool Bpv7AbstractSecurityBlockValueUint::IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const {
    if (const Bpv7AbstractSecurityBlockValueUint * asUintPtr = dynamic_cast<const Bpv7AbstractSecurityBlockValueUint*>(otherPtr)) {
        return (asUintPtr->m_uintValue == m_uintValue);
    }
    else {
        return false;
    }
}

Bpv7AbstractSecurityBlockValueByteString::~Bpv7AbstractSecurityBlockValueByteString() {}
uint64_t Bpv7AbstractSecurityBlockValueByteString::SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) {
    uint8_t * const byteStringHeaderStartPtr = serialization; //uint8_t * const serializationBase = serialization;
    uint64_t thisSerializationSize;
    thisSerializationSize = CborEncodeU64(serialization, m_byteString.size(), bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;
    *byteStringHeaderStartPtr |= (2U << 5); //change from major type 0 (unsigned integer) to major type 2 (byte string)

    if (bufferSize < m_byteString.size()) {
        return 0;
    }
    memcpy(serialization, m_byteString.data(), m_byteString.size());
    serialization += m_byteString.size(); //todo safety check on data length
    return serialization - byteStringHeaderStartPtr;// - serializationBase;
}
uint64_t Bpv7AbstractSecurityBlockValueByteString::GetSerializationSize() const {
    return CborGetEncodingSizeU64(m_byteString.size()) + m_byteString.size();
}
bool Bpv7AbstractSecurityBlockValueByteString::DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    if (bufferSize == 0) { //for byteStringHeader
        return false;
    }
    uint8_t * const byteStringHeaderStartPtr = serialization; //buffer size verified above
    const uint8_t cborMajorTypeByteString = (*byteStringHeaderStartPtr) >> 5;
    if (cborMajorTypeByteString != 2) {
        return false; //failure
    }
    *byteStringHeaderStartPtr &= 0x1f; //temporarily zero out major type to 0 to make it unsigned integer
    uint8_t cborSizeDecoded;
    const uint64_t dataLength = CborDecodeU64(byteStringHeaderStartPtr, &cborSizeDecoded, bufferSize);
    *byteStringHeaderStartPtr |= (2U << 5); // restore to major type to 2 (change from major type 0 (unsigned integer) to major type 2 (byte string))
    if (cborSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    if (dataLength > bufferSize) {
        return false;
    }
    m_byteString.assign(serialization, serialization + dataLength);
    serialization += dataLength;

    numBytesTakenToDecode = serialization - byteStringHeaderStartPtr;
    return true;
}
bool Bpv7AbstractSecurityBlockValueByteString::IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const {
    if (const Bpv7AbstractSecurityBlockValueByteString * asUintPtr = dynamic_cast<const Bpv7AbstractSecurityBlockValueByteString*>(otherPtr)) {
        return (asUintPtr->m_byteString == m_byteString);
    }
    else {
        return false;
    }
}
