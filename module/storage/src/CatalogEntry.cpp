/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "CatalogEntry.h"
#include <iostream>
#include <string>

catalog_entry_t::catalog_entry_t() :
    bundleSizeBytes(0),
    encodedAbsExpirationAndCustodyAndPriority(0),
    sequence(0),
    ptrUuidKeyInMap(NULL) { } //a default constructor: X()
catalog_entry_t::~catalog_entry_t() { } //a destructor: ~X()
catalog_entry_t::catalog_entry_t(const catalog_entry_t& o) :
    bundleSizeBytes(o.bundleSizeBytes),
    segmentIdChainVec(o.segmentIdChainVec),
    destEid(o.destEid),
    encodedAbsExpirationAndCustodyAndPriority(o.encodedAbsExpirationAndCustodyAndPriority),
    sequence(o.sequence),
    ptrUuidKeyInMap(o.ptrUuidKeyInMap) { } //a copy constructor: X(const X&)
catalog_entry_t::catalog_entry_t(catalog_entry_t&& o) :
    bundleSizeBytes(o.bundleSizeBytes),
    segmentIdChainVec(std::move(o.segmentIdChainVec)),
    destEid(o.destEid),
    encodedAbsExpirationAndCustodyAndPriority(o.encodedAbsExpirationAndCustodyAndPriority),
    sequence(o.sequence),
    ptrUuidKeyInMap(o.ptrUuidKeyInMap) { } //a move constructor: X(X&&)
catalog_entry_t& catalog_entry_t::operator=(const catalog_entry_t& o) { //a copy assignment: operator=(const X&)
    bundleSizeBytes = o.bundleSizeBytes;
    segmentIdChainVec = o.segmentIdChainVec;
    destEid = o.destEid;
    encodedAbsExpirationAndCustodyAndPriority = o.encodedAbsExpirationAndCustodyAndPriority;
    sequence = o.sequence;
    ptrUuidKeyInMap = o.ptrUuidKeyInMap;
    return *this;
}
catalog_entry_t& catalog_entry_t::operator=(catalog_entry_t && o) { //a move assignment: operator=(X&&)
    bundleSizeBytes = o.bundleSizeBytes;
    segmentIdChainVec = std::move(o.segmentIdChainVec);
    destEid = o.destEid;
    encodedAbsExpirationAndCustodyAndPriority = o.encodedAbsExpirationAndCustodyAndPriority;
    sequence = o.sequence;
    ptrUuidKeyInMap = o.ptrUuidKeyInMap;
    return *this;
}
bool catalog_entry_t::operator==(const catalog_entry_t & o) const {
    return
        (bundleSizeBytes == o.bundleSizeBytes) &&
        (segmentIdChainVec == o.segmentIdChainVec) &&
        (destEid == o.destEid) &&
        (encodedAbsExpirationAndCustodyAndPriority == o.encodedAbsExpirationAndCustodyAndPriority) &&
        (sequence == o.sequence) &&
        (ptrUuidKeyInMap == o.ptrUuidKeyInMap);
}
bool catalog_entry_t::operator!=(const catalog_entry_t & o) const {
    return
        (bundleSizeBytes != o.bundleSizeBytes) ||
        (segmentIdChainVec != o.segmentIdChainVec) ||
        (destEid != o.destEid) ||
        (encodedAbsExpirationAndCustodyAndPriority != o.encodedAbsExpirationAndCustodyAndPriority) ||
        (sequence != o.sequence) ||
        (ptrUuidKeyInMap != o.ptrUuidKeyInMap);
}
bool catalog_entry_t::operator<(const catalog_entry_t & o) const {
    return (segmentIdChainVec[0] < o.segmentIdChainVec[0]);
}
uint8_t catalog_entry_t::GetPriorityIndex() const {
    return static_cast<uint8_t>(encodedAbsExpirationAndCustodyAndPriority & 3);
}
uint64_t catalog_entry_t::GetAbsExpiration() const {
    return encodedAbsExpirationAndCustodyAndPriority >> 4;
}
bool catalog_entry_t::HasCustodyAndFragmentation() const {
    return ((encodedAbsExpirationAndCustodyAndPriority & (1U << 2)) != 0);
}
bool catalog_entry_t::HasCustodyAndNonFragmentation() const {
    return ((encodedAbsExpirationAndCustodyAndPriority & (1U << 3)) != 0);
}
bool catalog_entry_t::HasCustody() const {
    return ((encodedAbsExpirationAndCustodyAndPriority & ((1U << 2) | (1U << 3)) ) != 0);
}
void catalog_entry_t::Init(const PrimaryBlock & primary, const uint64_t paramBundleSizeBytes, const uint64_t paramNumSegmentsRequired, void * paramPtrUuidKeyInMap) {
    bundleSizeBytes = paramBundleSizeBytes;
    destEid = primary.GetFinalDestinationEid();
    encodedAbsExpirationAndCustodyAndPriority = primary.GetPriority() | (primary.GetExpirationSeconds() << 4);
    if (primary.HasCustodyFlagSet()) {
        if (primary.HasFragmentationFlagSet()) {
            encodedAbsExpirationAndCustodyAndPriority |= (1U << 2); //HasCustodyAndFragmentation
        }
        else {
            encodedAbsExpirationAndCustodyAndPriority |= (1U << 3); //HasCustodyAndNonFragmentation
        }
    }
    ptrUuidKeyInMap = paramPtrUuidKeyInMap;
    sequence = primary.GetSequenceForSecondsScale();
    segmentIdChainVec.resize(paramNumSegmentsRequired);
}

