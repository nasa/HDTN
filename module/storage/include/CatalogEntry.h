#ifndef _CATALOG_ENTRY_H
#define _CATALOG_ENTRY_H

#include <cstdint>
#include "MemoryManagerTreeArray.h"
#include "codec/bpv6.h"

struct catalog_entry_t {
    uint64_t bundleSizeBytes;
    segment_id_chain_vec_t segmentIdChainVec;
    cbhe_eid_t destEid;
    uint64_t encodedAbsExpirationAndCustodyAndPriority;
    uint64_t sequence;
    const void * ptrUuidKeyInMap;

    catalog_entry_t(); //a default constructor: X()
    ~catalog_entry_t(); //a destructor: ~X()
    catalog_entry_t(const catalog_entry_t& o); //a copy constructor: X(const X&)
    catalog_entry_t(catalog_entry_t&& o); //a move constructor: X(X&&)
    catalog_entry_t& operator=(const catalog_entry_t& o); //a copy assignment: operator=(const X&)
    catalog_entry_t& operator=(catalog_entry_t&& o); //a move assignment: operator=(X&&)
    bool operator==(const catalog_entry_t & o) const; //operator ==
    bool operator!=(const catalog_entry_t & o) const; //operator !=
    bool operator<(const catalog_entry_t & o) const; //operator < so it can be used as a map key

    uint8_t GetPriorityIndex() const ;
    uint64_t GetAbsExpiration() const;
    bool HasCustodyAndFragmentation() const;
    bool HasCustodyAndNonFragmentation() const;
    bool HasCustody() const;
    void Init(const bpv6_primary_block & primary, const uint64_t paramBundleSizeBytes, const uint64_t paramNumSegmentsRequired, void * paramPtrUuidKeyInMap);
};

#endif //_CATALOG_ENTRY_H
