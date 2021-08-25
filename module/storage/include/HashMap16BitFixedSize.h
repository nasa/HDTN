#ifndef _HASH_MAP_16BIT_FIXED_SIZE_H
#define _HASH_MAP_16BIT_FIXED_SIZE_H

#include <cstdint>
#include <forward_list>
#include <array>
#include <vector>
#include "codec/bpv6.h"

template <typename keyType, typename valueType>
class HashMap16BitFixedSize {
public:
    typedef std::pair<keyType, valueType> key_value_pair_t;
    typedef std::forward_list<key_value_pair_t> bucket_t;
    typedef std::array<bucket_t, 65536> bucket_array_t;


    HashMap16BitFixedSize();
    ~HashMap16BitFixedSize();

    static uint16_t GetHash(const cbhe_bundle_uuid_t & bundleUuid);
    static uint16_t GetHash(const cbhe_bundle_uuid_nofragment_t & bundleUuid);
    static uint16_t GetHash(const uint64_t key);

    //return true if inserted, false if already exists
    bool Insert(const keyType & key, const valueType & value);
    bool Insert(const keyType & key, valueType && value);
    bool Insert(const uint16_t hash, const keyType & key, const valueType & value);
    bool Insert(const uint16_t hash, const keyType & key, valueType && value);

    //return true if exists, false if key doesn't exist in the map
    bool GetValueAndRemove(const keyType & key, valueType & value);
    bool GetValueAndRemove(const uint16_t hash, const keyType & key, valueType & value);

    //return true if exists, false if key doesn't exist in the map
    valueType * GetValuePtr(const keyType & key);
    valueType * GetValuePtr(const uint16_t hash, const keyType & key);


    void BucketToVector(const uint16_t hash, std::vector<key_value_pair_t> & bucketAsVector);
    std::size_t GetBucketSize(const uint16_t hash);

    void Clear();


private:
    std::unique_ptr<bucket_array_t> m_bucketsPtr;
    bucket_array_t & m_buckets;
};


#endif //_HASH_MAP_16BIT_FIXED_SIZE_H
