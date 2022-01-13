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

////////////////////////////////////
// PREVIOUS NODE EXTENSION BLOCK
////////////////////////////////////

Bpv7PreviousNodeCanonicalBlock::Bpv7PreviousNodeCanonicalBlock() : Bpv7CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCKTYPE_PREVIOUS_NODE;
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
    m_blockTypeCode = BPV7_BLOCKTYPE_PREVIOUS_NODE;
    uint8_t tempData[largestSerializedDataOnlySize];
    m_dataPtr = tempData;
    m_dataLength = m_previousNode.SerializeBpv7(tempData);
    return Bpv7CanonicalBlock::SerializeBpv7(serialization);
}

bool Bpv7PreviousNodeCanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv7() {
    uint8_t numBytesTakenToDecode;
    return ((m_dataPtr != NULL) && (m_previousNode.DeserializeBpv7(m_dataPtr, &numBytesTakenToDecode, m_dataLength)) && (numBytesTakenToDecode == m_dataLength));
}

////////////////////////////////////
// BUNDLE AGE EXTENSION BLOCK
////////////////////////////////////

Bpv7BundleAgeCanonicalBlock::Bpv7BundleAgeCanonicalBlock() : Bpv7CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCKTYPE_BUNDLE_AGE;
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
    m_blockTypeCode = BPV7_BLOCKTYPE_BUNDLE_AGE;
    uint8_t tempData[largestSerializedDataOnlySize];
    m_dataPtr = tempData;
    m_dataLength = CborEncodeU64BufSize9(tempData, m_bundleAgeMilliseconds);
    return Bpv7CanonicalBlock::SerializeBpv7(serialization);
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
    m_blockTypeCode = BPV7_BLOCKTYPE_HOP_COUNT;
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
    m_blockTypeCode = BPV7_BLOCKTYPE_HOP_COUNT;
    uint8_t tempData[largestSerializedDataOnlySize];
    m_dataPtr = tempData;
    m_dataLength = CborTwoUint64ArraySerialize(tempData, m_hopLimit, m_hopCount);
    return Bpv7CanonicalBlock::SerializeBpv7(serialization);
}

bool Bpv7HopCountCanonicalBlock::TryReserializeExtensionBlockDataWithoutResizeBpv7() {
    //If hop count doesn't transition from 23 to 24, and hop limit doesn't change, then
    //this block can be updated in place without data resize modifications.
    //If successful, call blocks[0]->headerPtr->RecomputeCrcAfterDataModification((uint8_t*)blocks[0]->actualSerializedBlockPtr.data(), blocks[0]->actualSerializedBlockPtr.size());
    m_blockTypeCode = BPV7_BLOCKTYPE_HOP_COUNT;
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
