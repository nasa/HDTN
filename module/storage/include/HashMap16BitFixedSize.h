#ifndef _HASH_MAP_16BIT_FIXED_SIZE_H
#define _HASH_MAP_16BIT_FIXED_SIZE_H

#include <cstdint>
#include <forward_list>
#include <array>
#include <vector>
#include "codec/bpv6.h"
#include <memory>
#include "storage_lib_export.h"

template <typename keyType, typename valueType>
class HashMap16BitFixedSize {
public:
    typedef std::pair<keyType, valueType> key_value_pair_t;
    typedef std::forward_list<key_value_pair_t> bucket_t;
    typedef std::array<bucket_t, 65536> bucket_array_t;


    STORAGE_LIB_EXPORT HashMap16BitFixedSize();
    STORAGE_LIB_EXPORT ~HashMap16BitFixedSize();

    STORAGE_LIB_EXPORT static uint16_t GetHash(const cbhe_bundle_uuid_t & bundleUuid);
    STORAGE_LIB_EXPORT static uint16_t GetHash(const cbhe_bundle_uuid_nofragment_t & bundleUuid);
    STORAGE_LIB_EXPORT static uint16_t GetHash(const uint64_t key);

    //return ptr of inserted pair if inserted, NULL if already exists
    STORAGE_LIB_EXPORT const key_value_pair_t * Insert(const keyType & key, const valueType & value);
    STORAGE_LIB_EXPORT const key_value_pair_t * Insert(const keyType & key, valueType && value);
    STORAGE_LIB_EXPORT const key_value_pair_t * Insert(const uint16_t hash, const keyType & key, const valueType & value);
    STORAGE_LIB_EXPORT const key_value_pair_t * Insert(const uint16_t hash, const keyType & key, valueType && value);

    //return true if exists, false if key doesn't exist in the map
    STORAGE_LIB_EXPORT bool GetValueAndRemove(const keyType & key, valueType & value);
    STORAGE_LIB_EXPORT bool GetValueAndRemove(const uint16_t hash, const keyType & key, valueType & value);

    //return ptr if exists, NULL if key doesn't exist in the map
    STORAGE_LIB_EXPORT valueType * GetValuePtr(const keyType & key);
    STORAGE_LIB_EXPORT valueType * GetValuePtr(const uint16_t hash, const keyType & key);


    STORAGE_LIB_EXPORT void BucketToVector(const uint16_t hash, std::vector<key_value_pair_t> & bucketAsVector);
    STORAGE_LIB_EXPORT std::size_t GetBucketSize(const uint16_t hash);

    STORAGE_LIB_EXPORT void Clear();


private:
    std::unique_ptr<bucket_array_t> m_bucketsPtr;
    bucket_array_t & m_buckets;
};


#endif //_HASH_MAP_16BIT_FIXED_SIZE_H
