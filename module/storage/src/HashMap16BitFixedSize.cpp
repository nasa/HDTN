/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "HashMap16BitFixedSize.h"
#include <nmmintrin.h>
#include <boost/make_unique.hpp>
#include <iostream>
#include "CatalogEntry.h"

template <typename keyType, typename valueType>
HashMap16BitFixedSize<keyType, valueType>::HashMap16BitFixedSize() :
    m_bucketsPtr(boost::make_unique<bucket_array_t>()),
    m_buckets(*m_bucketsPtr) {}

template <typename keyType, typename valueType>
HashMap16BitFixedSize<keyType, valueType>::~HashMap16BitFixedSize() {}

template <typename keyType, typename valueType>
uint16_t HashMap16BitFixedSize<keyType, valueType>::GetHash(const cbhe_bundle_uuid_t & bundleUuid) {
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

template <typename keyType, typename valueType>
uint16_t HashMap16BitFixedSize<keyType, valueType>::GetHash(const cbhe_bundle_uuid_nofragment_t & bundleUuid) {
    const uint64_t v1 = bundleUuid.creationSeconds;
    const uint64_t v2 = bundleUuid.sequence;
    const uint64_t v3 = bundleUuid.srcEid.nodeId;
    const uint64_t v4 = bundleUuid.srcEid.serviceId;

    const uint32_t crc32 = static_cast<uint32_t>(_mm_crc32_u64(_mm_crc32_u64(_mm_crc32_u64(_mm_crc32_u64(UINT32_MAX, v1), v2), v3), v4));
    return (static_cast<uint16_t>(crc32 >> 16)) ^ (static_cast<uint16_t>(crc32));
}

template <typename keyType, typename valueType>
uint16_t HashMap16BitFixedSize<keyType, valueType>::GetHash(const uint64_t key) {
    return static_cast<uint16_t>(key);
}

//insert into buckets in order
//return true if inserted, false if already exists
template <typename keyType, typename valueType>
bool HashMap16BitFixedSize<keyType, valueType>::Insert(const keyType & key, const valueType & value) {
    return Insert(GetHash(key), key, std::move(valueType(value)));
}

//insert into buckets in order
//return true if inserted, false if already exists
template <typename keyType, typename valueType>
bool HashMap16BitFixedSize<keyType, valueType>::Insert(const keyType & key, valueType && value) {
    return Insert(GetHash(key), key, std::move(value));
}

//insert into buckets in order
//return true if inserted, false if already exists
template <typename keyType, typename valueType>
bool HashMap16BitFixedSize<keyType, valueType>::Insert(const uint16_t hash, const keyType & key, const valueType & value) {
    return Insert(hash, key, std::move(valueType(value)));
}

//insert into buckets in order
//return true if inserted, false if already exists
template <typename keyType, typename valueType>
bool HashMap16BitFixedSize<keyType, valueType>::Insert(const uint16_t hash, const keyType & key, valueType && value) {
    bucket_t & bucket = m_buckets[hash];
    if (bucket.empty()) {
        bucket.emplace_front(key, std::move(value));
        return true;
    }
    else {
        bucket_t::const_iterator itPrev = bucket.cbefore_begin();
        for (bucket_t::const_iterator it = bucket.cbegin(); it != bucket.cend(); ++it, ++itPrev) {
            const keyType & keyInList = it->first;
            if (key < keyInList) { //not in list, insert now
                break; //will call bucket.emplace_after(itPrev, key, std::move(value)); and return true;
            }
            else if (keyInList < key) { //greater than element
                continue;
            }
            else { //equal, already exists
                return false;
            }
        }
        //uuid is greater than every element in the bucket, insert at the end
        bucket.emplace_after(itPrev, key, std::move(value));
        return true;
    }
}

//return true if exists, false if key doesn't exist in the map
template <typename keyType, typename valueType>
bool HashMap16BitFixedSize<keyType, valueType>::GetValueAndRemove(const keyType & key, valueType & value) {
    return GetValueAndRemove(GetHash(key), key, value);
}

//return true if exists, false if key doesn't exist in the map
template <typename keyType, typename valueType>
bool HashMap16BitFixedSize<keyType, valueType>::GetValueAndRemove(const uint16_t hash, const keyType & key, valueType & value) {
    bucket_t & bucket = m_buckets[hash];
    if (bucket.empty()) {
        return false;
    }
    else {
        //iterators must be non-const or the move below will just result in a copy (still compiles)
        for (bucket_t::iterator itPrev = bucket.before_begin(), it = bucket.begin(); it != bucket.end(); ++it, ++itPrev) {
            const keyType & keyInList = it->first;
            if (key < keyInList) { //not in list, therefore not found
                return false;
            }
            else if (keyInList < key) { //greater than element
                continue;
            }
            else { //equal, found
                value = std::move(it->second);
                bucket.erase_after(itPrev);
                return true;
            }
        }
        //uuid is greater than every element in the bucket, therefore not found
        return false;
    }
}

//return true if exists, false if key doesn't exist in the map
template <typename keyType, typename valueType>
valueType *  HashMap16BitFixedSize<keyType, valueType>::GetValuePtr(const keyType & key) {
    return GetValuePtr(GetHash(key), key);
}

//return true if exists, false if key doesn't exist in the map
template <typename keyType, typename valueType>
valueType * HashMap16BitFixedSize<keyType, valueType>::GetValuePtr(const uint16_t hash, const keyType & key) {
    bucket_t & bucket = m_buckets[hash];
    if (bucket.empty()) {
        return NULL;
    }
    else {
        //iterators must be non-const or the move below will just result in a copy (still compiles)
        for (bucket_t::iterator it = bucket.begin(); it != bucket.end(); ++it) {
            const keyType & keyInList = it->first;
            if (key < keyInList) { //not in list, therefore not found
                return NULL;
            }
            else if (keyInList < key) { //greater than element
                continue;
            }
            else { //equal, found
                return &(it->second);
            }
        }
        //uuid is greater than every element in the bucket, therefore not found
        return false;
    }
}

template <typename keyType, typename valueType>
void HashMap16BitFixedSize<keyType, valueType>::Clear() {
    for (std::size_t i = 0; i < m_buckets.size(); ++i) {
        m_buckets[i].clear();
    }
}

template <typename keyType, typename valueType>
void HashMap16BitFixedSize<keyType, valueType>::BucketToVector(const uint16_t hash, std::vector<key_value_pair_t> & bucketAsVector) {
    bucketAsVector.resize(0);
    bucket_t & bucket = m_buckets[hash];
    for (bucket_t::const_iterator it = bucket.cbegin(); it != bucket.cend(); ++it) {
        bucketAsVector.push_back(*it);
    }
}

template <typename keyType, typename valueType>
std::size_t HashMap16BitFixedSize<keyType, valueType>::GetBucketSize(const uint16_t hash) {
    bucket_t & bucket = m_buckets[hash];
    std::size_t size = 0;
    for (bucket_t::const_iterator it = bucket.cbegin(); it != bucket.cend(); ++it) {
        ++size;
    }
    return size;
}

// Explicit template instantiation
template class HashMap16BitFixedSize<cbhe_bundle_uuid_t, uint64_t>;
template class HashMap16BitFixedSize<cbhe_bundle_uuid_nofragment_t, uint64_t>;
template class HashMap16BitFixedSize<uint64_t, catalog_entry_t>;

