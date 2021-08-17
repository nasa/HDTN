/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "BundleUuidToUint64HashMap.h"
#include <nmmintrin.h>
#include <boost/make_unique.hpp>
#include <iostream>

template <typename uuidType>
BundleUuidToUint64HashMap<uuidType>::BundleUuidToUint64HashMap() : 
    m_bucketsPtr(boost::make_unique<bucket_array_t>()),
    m_buckets(*m_bucketsPtr) {}

template <typename uuidType>
BundleUuidToUint64HashMap<uuidType>::~BundleUuidToUint64HashMap() {}

template <typename uuidType>
uint16_t BundleUuidToUint64HashMap<uuidType>::GetHash(const cbhe_bundle_uuid_t & bundleUuid) {
    const uint64_t v1 = bundleUuid.creationSeconds;
    const uint64_t v2 = bundleUuid.sequence;
    const uint64_t v3 = bundleUuid.srcEid.nodeId;
    const uint64_t v4 = bundleUuid.srcEid.serviceId;
    const uint64_t v5 = bundleUuid.fragmentOffset;
    const uint64_t v6 = bundleUuid.dataLength;
    
    const uint32_t crc32 = static_cast<uint32_t>(
        _mm_crc32_u64(_mm_crc32_u64(_mm_crc32_u64(_mm_crc32_u64(_mm_crc32_u64(_mm_crc32_u64(UINT32_MAX, v1), v2), v3), v4), v5), v6)
    );
    return (static_cast<uint16_t>(crc32 >> 16)) ^ (static_cast<uint16_t>(crc32));
}

template <typename uuidType>
uint16_t BundleUuidToUint64HashMap<uuidType>::GetHash(const cbhe_bundle_uuid_nofragment_t & bundleUuid) {
    const uint64_t v1 = bundleUuid.creationSeconds;
    const uint64_t v2 = bundleUuid.sequence;
    const uint64_t v3 = bundleUuid.srcEid.nodeId;
    const uint64_t v4 = bundleUuid.srcEid.serviceId;

    const uint32_t crc32 = static_cast<uint32_t>(_mm_crc32_u64(_mm_crc32_u64(_mm_crc32_u64(_mm_crc32_u64(UINT32_MAX, v1), v2), v3), v4));
    return (static_cast<uint16_t>(crc32 >> 16)) ^ (static_cast<uint16_t>(crc32));
}

//insert into buckets in order
//return true if inserted, false if already exists
template <typename uuidType>
bool BundleUuidToUint64HashMap<uuidType>::Insert(const uuidType & bundleUuid, uint64_t value) {
    return Insert(GetHash(bundleUuid), bundleUuid, value);
}

//insert into buckets in order
//return true if inserted, false if already exists
template <typename uuidType>
bool BundleUuidToUint64HashMap<uuidType>::Insert(const uint16_t hash, const uuidType & bundleUuid, uint64_t value) {
    bucket_t & bucket = m_buckets[hash];
    if (bucket.empty()) {
        bucket.push_front(pair_uuid_uint64_t(bundleUuid, value));
        return true;
    }
    else {
        bucket_t::const_iterator itPrev = bucket.cbefore_begin();
        for (bucket_t::const_iterator it = bucket.cbegin(); it != bucket.cend(); ++it, ++itPrev) {
            const uuidType & bundleUuidInList = it->first;
            if (bundleUuid < bundleUuidInList) { //not in list, insert now
                bucket.insert_after(itPrev, pair_uuid_uint64_t(bundleUuid, value));
                return true;
            }
            else if (bundleUuidInList < bundleUuid) { //greater than element
                continue;
            }
            else { //equal, already exists
                return false;
            }
        }
        //uuid is greater than every element in the bucket, insert at the end
        bucket.insert_after(itPrev, pair_uuid_uint64_t(bundleUuid, value));
        return true;
    }
}

//return true if exists, false if key doesn't exist in the map
template <typename uuidType>
bool BundleUuidToUint64HashMap<uuidType>::GetValueAndRemove(const uuidType & bundleUuid, uint64_t & value) {
    return GetValueAndRemove(GetHash(bundleUuid), bundleUuid, value);
}

//return true if exists, false if key doesn't exist in the map
template <typename uuidType>
bool BundleUuidToUint64HashMap<uuidType>::GetValueAndRemove(const uint16_t hash, const uuidType & bundleUuid, uint64_t & value) {
    bucket_t & bucket = m_buckets[hash];
    if (bucket.empty()) {
        return false;
    }
    else {
        for (bucket_t::const_iterator itPrev = bucket.cbefore_begin(), it = bucket.cbegin(); it != bucket.cend(); ++it, ++itPrev) {
            const uuidType & bundleUuidInList = it->first;
            if (bundleUuid < bundleUuidInList) { //not in list, therefore not found
                return false;
            }
            else if (bundleUuidInList < bundleUuid) { //greater than element
                continue;
            }
            else { //equal, found
                value = it->second;
                bucket.erase_after(itPrev);
                return true;
            }
        }
        //uuid is greater than every element in the bucket, therefore not found
        return false;
    }
}

template <typename uuidType>
void BundleUuidToUint64HashMap<uuidType>::Clear() {
    for (std::size_t i = 0; i < m_buckets.size(); ++i) {
        m_buckets[i].clear();
    }
}

template <typename uuidType>
void BundleUuidToUint64HashMap<uuidType>::BucketToVector(const uint16_t hash, std::vector<pair_uuid_uint64_t> & bucketAsVector) {
    bucketAsVector.resize(0);
    bucket_t & bucket = m_buckets[hash];
    for (bucket_t::const_iterator it = bucket.cbegin(); it != bucket.cend(); ++it) {
        bucketAsVector.push_back(*it);
    }
}

template <typename uuidType>
std::size_t BundleUuidToUint64HashMap<uuidType>::GetBucketSize(const uint16_t hash) {
    bucket_t & bucket = m_buckets[hash];
    std::size_t size = 0;
    for (bucket_t::const_iterator it = bucket.cbegin(); it != bucket.cend(); ++it) {
        ++size;
    }
    return size;
}

// Explicit template instantiation
template class BundleUuidToUint64HashMap<cbhe_bundle_uuid_t>;
template class BundleUuidToUint64HashMap<cbhe_bundle_uuid_nofragment_t>;


