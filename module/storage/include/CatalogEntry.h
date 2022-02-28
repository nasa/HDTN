#ifndef _CATALOG_ENTRY_H
#define _CATALOG_ENTRY_H

#include <cstdint>
#include "MemoryManagerTreeArray.h"
#include "codec/PrimaryBlock.h"
#include "storage_lib_export.h"

struct catalog_entry_t {
    uint64_t bundleSizeBytes;
    segment_id_chain_vec_t segmentIdChainVec;
    cbhe_eid_t destEid;
    uint64_t encodedAbsExpirationAndCustodyAndPriority;
    uint64_t sequence;
    const void * ptrUuidKeyInMap;

    STORAGE_LIB_EXPORT catalog_entry_t(); //a default constructor: X()
    STORAGE_LIB_EXPORT ~catalog_entry_t(); //a destructor: ~X()
    STORAGE_LIB_EXPORT catalog_entry_t(const catalog_entry_t& o); //a copy constructor: X(const X&)
    STORAGE_LIB_EXPORT catalog_entry_t(catalog_entry_t&& o); //a move constructor: X(X&&)
    STORAGE_LIB_EXPORT catalog_entry_t& operator=(const catalog_entry_t& o); //a copy assignment: operator=(const X&)
    STORAGE_LIB_EXPORT catalog_entry_t& operator=(catalog_entry_t&& o); //a move assignment: operator=(X&&)
    STORAGE_LIB_EXPORT bool operator==(const catalog_entry_t & o) const; //operator ==
    STORAGE_LIB_EXPORT bool operator!=(const catalog_entry_t & o) const; //operator !=
    STORAGE_LIB_EXPORT bool operator<(const catalog_entry_t & o) const; //operator < so it can be used as a map key

    STORAGE_LIB_EXPORT uint8_t GetPriorityIndex() const ;
    STORAGE_LIB_EXPORT uint64_t GetAbsExpiration() const;
    STORAGE_LIB_EXPORT bool HasCustodyAndFragmentation() const;
    STORAGE_LIB_EXPORT bool HasCustodyAndNonFragmentation() const;
    STORAGE_LIB_EXPORT bool HasCustody() const;
    STORAGE_LIB_EXPORT void Init(const PrimaryBlock & primary, const uint64_t paramBundleSizeBytes, const uint64_t paramNumSegmentsRequired, void * paramPtrUuidKeyInMap);
};

#endif //_CATALOG_ENTRY_H
