#ifndef _BUNDLE_UUID_TO_UINT64_HASH_MAP_H
#define _BUNDLE_UUID_TO_UINT64_HASH_MAP_H

#include <cstdint>
#include <forward_list>
#include <array>
#include <vector>
#include "codec/bpv6.h"

template <typename uuidType>
class BundleUuidToUint64HashMap {
public:
    typedef std::pair<uuidType, uint64_t> pair_uuid_uint64_t;
    typedef std::forward_list<pair_uuid_uint64_t> bucket_t;
    typedef std::array<bucket_t, 65536> bucket_array_t;


    BundleUuidToUint64HashMap();
    ~BundleUuidToUint64HashMap();

    static uint16_t GetHash(const cbhe_bundle_uuid_t & bundleUuid);
    static uint16_t GetHash(const cbhe_bundle_uuid_nofragment_t & bundleUuid);

    //return true if inserted, false if already exists
    bool Insert(const uuidType & bundleUuid, uint64_t value);
    bool Insert(const uint16_t hash, const uuidType & bundleUuid, uint64_t value);

    //return true if exists, false if key doesn't exist in the map
    bool GetValueAndRemove(const uuidType & bundleUuid, uint64_t & value);
    bool GetValueAndRemove(const uint16_t hash, const uuidType & bundleUuid, uint64_t & value);

    void BucketToVector(const uint16_t hash, std::vector<pair_uuid_uint64_t> & bucketAsVector);
    std::size_t GetBucketSize(const uint16_t hash);

    void Clear();


private:
    std::unique_ptr<bucket_array_t> m_bucketsPtr;
    bucket_array_t & m_buckets;
};


#endif //_BUNDLE_UUID_TO_UINT64_HASH_MAP_H
