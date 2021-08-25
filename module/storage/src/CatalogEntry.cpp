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
    encodedAbsExpirationAndPriorityIndex(0),
    sequence(0) { } //a default constructor: X()
catalog_entry_t::catalog_entry_t(const bpv6_primary_block & primary) :
    bundleSizeBytes(0),
    destEid(primary.dst_node, primary.dst_svc),
    encodedAbsExpirationAndPriorityIndex(bpv6_bundle_get_priority(primary.flags) | ((primary.creation + primary.lifetime) << 2)),
    sequence(primary.sequence) { }
catalog_entry_t::~catalog_entry_t() { } //a destructor: ~X()
catalog_entry_t::catalog_entry_t(const catalog_entry_t& o) :
    bundleSizeBytes(o.bundleSizeBytes),
    segmentIdChainVec(o.segmentIdChainVec),
    destEid(o.destEid),
    encodedAbsExpirationAndPriorityIndex(o.encodedAbsExpirationAndPriorityIndex),
    sequence(o.sequence) { } //a copy constructor: X(const X&)
catalog_entry_t::catalog_entry_t(catalog_entry_t&& o) :
    bundleSizeBytes(o.bundleSizeBytes),
    segmentIdChainVec(std::move(o.segmentIdChainVec)),
    destEid(o.destEid),
    encodedAbsExpirationAndPriorityIndex(o.encodedAbsExpirationAndPriorityIndex),
    sequence(o.sequence) { } //a move constructor: X(X&&)
catalog_entry_t& catalog_entry_t::operator=(const catalog_entry_t& o) { //a copy assignment: operator=(const X&)
    bundleSizeBytes = o.bundleSizeBytes;
    segmentIdChainVec = o.segmentIdChainVec;
    destEid = o.destEid;
    encodedAbsExpirationAndPriorityIndex = o.encodedAbsExpirationAndPriorityIndex;
    sequence = o.sequence;
    return *this;
}
catalog_entry_t& catalog_entry_t::operator=(catalog_entry_t && o) { //a move assignment: operator=(X&&)
    bundleSizeBytes = o.bundleSizeBytes;
    segmentIdChainVec = std::move(o.segmentIdChainVec);
    destEid = o.destEid;
    encodedAbsExpirationAndPriorityIndex = o.encodedAbsExpirationAndPriorityIndex;
    sequence = o.sequence;
    return *this;
}
bool catalog_entry_t::operator==(const catalog_entry_t & o) const {
    return
        (bundleSizeBytes == o.bundleSizeBytes) &&
        (segmentIdChainVec == o.segmentIdChainVec) &&
        (destEid == o.destEid) &&
        (encodedAbsExpirationAndPriorityIndex == o.encodedAbsExpirationAndPriorityIndex) &&
        (sequence == o.sequence);
}
bool catalog_entry_t::operator!=(const catalog_entry_t & o) const {
    return
        (bundleSizeBytes != o.bundleSizeBytes) ||
        (segmentIdChainVec != o.segmentIdChainVec) ||
        (destEid != o.destEid) ||
        (encodedAbsExpirationAndPriorityIndex != o.encodedAbsExpirationAndPriorityIndex) ||
        (sequence != o.sequence);
}
bool catalog_entry_t::operator<(const catalog_entry_t & o) const {
    return (segmentIdChainVec[0] < o.segmentIdChainVec[0]);
}
void catalog_entry_t::SetAbsExpirationAndPriority(uint8_t priorityIndex, uint64_t expiration) {
    encodedAbsExpirationAndPriorityIndex = (expiration << 2) | priorityIndex;
}
uint8_t catalog_entry_t::GetPriorityIndex() const {
    return static_cast<uint8_t>(encodedAbsExpirationAndPriorityIndex & 3);
}
uint64_t catalog_entry_t::GetAbsExpiration() const {
    return encodedAbsExpirationAndPriorityIndex >> 2;
}



