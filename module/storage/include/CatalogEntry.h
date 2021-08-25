#ifndef _CATALOG_ENTRY_H
#define _CATALOG_ENTRY_H

#include <cstdint>
#include "MemoryManagerTreeArray.h"
#include "codec/bpv6.h"

struct catalog_entry_t {
    uint64_t bundleSizeBytes;
    segment_id_chain_vec_t segmentIdChainVec;
    cbhe_eid_t destEid;
    uint64_t encodedAbsExpirationAndPriorityIndex;
    uint64_t sequence;

    catalog_entry_t(); //a default constructor: X()
    catalog_entry_t(const bpv6_primary_block & bundlePrimaryBlock);
    ~catalog_entry_t(); //a destructor: ~X()
    catalog_entry_t(const catalog_entry_t& o); //a copy constructor: X(const X&)
    catalog_entry_t(catalog_entry_t&& o); //a move constructor: X(X&&)
    catalog_entry_t& operator=(const catalog_entry_t& o); //a copy assignment: operator=(const X&)
    catalog_entry_t& operator=(catalog_entry_t&& o); //a move assignment: operator=(X&&)
    bool operator==(const catalog_entry_t & o) const; //operator ==
    bool operator!=(const catalog_entry_t & o) const; //operator !=
    bool operator<(const catalog_entry_t & o) const; //operator < so it can be used as a map key

    void SetAbsExpirationAndPriority(uint8_t priorityIndex, uint64_t expiration);
    uint8_t GetPriorityIndex() const ;
    uint64_t GetAbsExpiration() const;
};

#endif //_CATALOG_ENTRY_H
