#include <boost/test/unit_test.hpp>
#include "HashMap16BitFixedSize.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <set>

template <class uuidType>
static void DoTest() {
    typedef typename HashMap16BitFixedSize<uuidType, uint64_t>::key_value_pair_t uuid_u64_t;
    const std::vector<uuid_u64_t> bundleUuidPlusU64Vec({
        uuid_u64_t(cbhe_bundle_uuid_t(
            1000, //creationSeconds
            1, // sequence,
            10, // srcNodeId,
            20, // srcServiceId,
            0, // fragmentOffset,
            0 // dataLength
        ), 1),
        uuid_u64_t(cbhe_bundle_uuid_t(
            1000, //creationSeconds
            2, // sequence,
            10, // srcNodeId,
            20, // srcServiceId,
            0, // fragmentOffset,
            0 // dataLength
        ), 2),
        uuid_u64_t(cbhe_bundle_uuid_t(
            1000, //creationSeconds
            3, // sequence,
            10, // srcNodeId,
            20, // srcServiceId,
            0, // fragmentOffset,
            0 // dataLength
        ), 3),
        uuid_u64_t(cbhe_bundle_uuid_t(
            1000, //creationSeconds
            4, // sequence,
            10, // srcNodeId,
            20, // srcServiceId,
            0, // fragmentOffset,
            0 // dataLength
        ), 4)
        });

    //4 bundle uuids should produce different hashes
    {
        std::set<uint16_t> hashSet;
        for (std::size_t i = 0; i < bundleUuidPlusU64Vec.size(); ++i) {
            uint16_t hash = HashMap16BitFixedSize<uuidType, uint64_t>::GetHash(bundleUuidPlusU64Vec[i].first);
            std::cout << hash << std::endl;
            BOOST_REQUIRE(hashSet.insert(hash).second);
        }
        BOOST_REQUIRE_EQUAL(hashSet.size(), bundleUuidPlusU64Vec.size());
    }

    //insert into bucket 1 in order, make sure values in bucket are read back in order
    {
        const uint16_t HASH = 1; //bypass hashing algorithm (assume these go to the same bucket)
        HashMap16BitFixedSize<uuidType, uint64_t> hm;
        for (std::size_t i = 0; i < bundleUuidPlusU64Vec.size(); ++i) {
            const typename HashMap16BitFixedSize<uuidType, uint64_t>::key_value_pair_t * p = hm.Insert(HASH, bundleUuidPlusU64Vec[i].first, bundleUuidPlusU64Vec[i].second);
            BOOST_REQUIRE(p != NULL);
            BOOST_REQUIRE(p->first == bundleUuidPlusU64Vec[i].first);
            BOOST_REQUIRE(p->second == bundleUuidPlusU64Vec[i].second);
        }
        std::vector<uuid_u64_t> bucketAsVector;
        hm.BucketToVector(HASH, bucketAsVector);
        BOOST_REQUIRE(bundleUuidPlusU64Vec == bucketAsVector);
    }

    //insert into bucket 1 out-of-order, make sure values in bucket are read back in order
    {
        const uint16_t HASH = 1; //bypass hashing algorithm (assume these go to the same bucket)
        HashMap16BitFixedSize<uuidType, uint64_t> hm;
        for (int64_t i = static_cast<int64_t>(bundleUuidPlusU64Vec.size() - 1); i >= 0; --i) {
            const typename HashMap16BitFixedSize<uuidType, uint64_t>::key_value_pair_t * p = hm.Insert(HASH, bundleUuidPlusU64Vec[i].first, bundleUuidPlusU64Vec[i].second);
            BOOST_REQUIRE(p != NULL);
            BOOST_REQUIRE(p->first == bundleUuidPlusU64Vec[i].first);
            BOOST_REQUIRE(p->second == bundleUuidPlusU64Vec[i].second);
        }
        std::vector<uuid_u64_t> bucketAsVector;
        hm.BucketToVector(HASH, bucketAsVector);
        BOOST_REQUIRE(bundleUuidPlusU64Vec == bucketAsVector);
    }

    //insert into bucket 1 in order (two times), second time failing, make sure values in bucket are read back in order
    {
        const uint16_t HASH = 1; //bypass hashing algorithm (assume these go to the same bucket)
        HashMap16BitFixedSize<uuidType, uint64_t> hm;
        for (std::size_t i = 0; i < bundleUuidPlusU64Vec.size(); ++i) {
            const typename HashMap16BitFixedSize<uuidType, uint64_t>::key_value_pair_t * p = hm.Insert(HASH, bundleUuidPlusU64Vec[i].first, bundleUuidPlusU64Vec[i].second);
            BOOST_REQUIRE(p != NULL);
            BOOST_REQUIRE(p->first == bundleUuidPlusU64Vec[i].first);
            BOOST_REQUIRE(p->second == bundleUuidPlusU64Vec[i].second);
        }
        for (std::size_t i = 0; i < bundleUuidPlusU64Vec.size(); ++i) {
            const typename HashMap16BitFixedSize<uuidType, uint64_t>::key_value_pair_t * p = hm.Insert(HASH, bundleUuidPlusU64Vec[i].first, bundleUuidPlusU64Vec[i].second);
            BOOST_REQUIRE(p == NULL);
        }
        std::vector<uuid_u64_t> bucketAsVector;
        hm.BucketToVector(HASH, bucketAsVector);
        BOOST_REQUIRE(bundleUuidPlusU64Vec == bucketAsVector);
    }

    //insert into bucket 1 in order (two times), second time failing, using real hash (will be 1 elem per bucket)
    {
        HashMap16BitFixedSize<uuidType, uint64_t> hm;
        for (std::size_t i = 0; i < bundleUuidPlusU64Vec.size(); ++i) {
            const typename HashMap16BitFixedSize<uuidType, uint64_t>::key_value_pair_t * p = hm.Insert(bundleUuidPlusU64Vec[i].first, bundleUuidPlusU64Vec[i].second);
            BOOST_REQUIRE(p != NULL);
            BOOST_REQUIRE(p->first == bundleUuidPlusU64Vec[i].first);
            BOOST_REQUIRE(p->second == bundleUuidPlusU64Vec[i].second);
        }
        for (std::size_t i = 0; i < bundleUuidPlusU64Vec.size(); ++i) {
            const typename HashMap16BitFixedSize<uuidType, uint64_t>::key_value_pair_t * p = hm.Insert(bundleUuidPlusU64Vec[i].first, bundleUuidPlusU64Vec[i].second);
            BOOST_REQUIRE(p == NULL);
        }
    }

    //insert elem 0 using real hash and remove it using pointer returned by insert
    {
        HashMap16BitFixedSize<uuidType, uint64_t> hm;
        uint64_t value = 0;
        BOOST_REQUIRE(!hm.GetValueAndRemove(bundleUuidPlusU64Vec[0].first, value)); //nothing here
        const typename HashMap16BitFixedSize<uuidType, uint64_t>::key_value_pair_t * p = hm.Insert(bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second); //insert 0
        BOOST_REQUIRE(p != NULL);
        BOOST_REQUIRE(p->first == bundleUuidPlusU64Vec[0].first);
        BOOST_REQUIRE(p->second == bundleUuidPlusU64Vec[0].second);
        BOOST_REQUIRE(hm.GetValueAndRemove(p->first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE(!hm.GetValueAndRemove(bundleUuidPlusU64Vec[0].first, value)); //fail remove 0 since already removed
    }

    //insert and deletion tests
    {
        const uint16_t HASH = 1; //bypass hashing algorithm (assume these go to the same bucket)
        HashMap16BitFixedSize<uuidType, uint64_t> hm;
        uint64_t value = 0;
        std::vector<uuid_u64_t> bucketAsVector;

        //insert elem 0 and remove it
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        

        //insert elem 0,1 and remove 0,1
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[1].first, bundleUuidPlusU64Vec[1].second)); //insert 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //remove 1
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[1].second, value); //verify value 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        //insert elem 0,1 and remove 1,0
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[1].first, bundleUuidPlusU64Vec[1].second)); //insert 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //remove 1
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[1].second, value); //verify value 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        //insert elem 0,1.2 and (fail to remove 3) and remove 0,1,2 
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[1].first, bundleUuidPlusU64Vec[1].second)); //insert 1
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[2].first, bundleUuidPlusU64Vec[2].second)); //insert 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[3].first, value)); //fail remove 3
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify still size 3
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //remove 1
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[1].second, value); //verify value 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[2].first, value)); //remove 2
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[2].second, value); //verify value 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        //insert elem 0,1.2 and remove 0,2,1
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[1].first, bundleUuidPlusU64Vec[1].second)); //insert 1
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[2].first, bundleUuidPlusU64Vec[2].second)); //insert 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[2].first, value)); //remove 2
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[2].second, value); //verify value 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //remove 1
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[1].second, value); //verify value 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        //insert elem 0,1.2 and remove 1,0,2
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[1].first, bundleUuidPlusU64Vec[1].second)); //insert 1
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[2].first, bundleUuidPlusU64Vec[2].second)); //insert 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //remove 1
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[1].second, value); //verify value 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[2].first, value)); //remove 2
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[2].second, value); //verify value 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        //insert elem 0,1.2 and remove 1,2,0
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[1].first, bundleUuidPlusU64Vec[1].second)); //insert 1
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[2].first, bundleUuidPlusU64Vec[2].second)); //insert 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //remove 1
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[1].second, value); //verify value 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[2].first, value)); //remove 2
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[2].second, value); //verify value 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        //insert elem 0,1.2 and remove 2,1,0
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[1].first, bundleUuidPlusU64Vec[1].second)); //insert 1
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[2].first, bundleUuidPlusU64Vec[2].second)); //insert 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[2].first, value)); //remove 2
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[2].second, value); //verify value 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //remove 1
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[1].second, value); //verify value 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        //insert elem 0,1.2 and remove 2,0,1
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[1].first, bundleUuidPlusU64Vec[1].second)); //insert 1
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[2].first, bundleUuidPlusU64Vec[2].second)); //insert 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[2].first, value)); //remove 2
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[2].second, value); //verify value 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //remove 1
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[1].second, value); //verify value 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        ////////////

        //insert elem 1,2,3 and (fail to remove 0) and remove 1,2,3 
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[1].first, bundleUuidPlusU64Vec[1].second)); //insert 1
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[2].first, bundleUuidPlusU64Vec[2].second)); //insert 2
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[3].first, bundleUuidPlusU64Vec[3].second)); //insert 3
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //fail remove 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify still size 3
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //remove 1
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[1].second, value); //verify value 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[2].first, value)); //remove 2
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[2].second, value); //verify value 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[3].first, value)); //remove 3
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[3].second, value); //verify value 3
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        //insert elem 0,2,3 and (fail to remove 1) and remove 0,2,3 
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //nothing here
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[2].first, bundleUuidPlusU64Vec[2].second)); //insert 2
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[3].first, bundleUuidPlusU64Vec[3].second)); //insert 3
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3
        BOOST_REQUIRE(!hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[1].first, value)); //fail remove 1
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify still size 3
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[0].first, value)); //remove 0
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[0].second, value); //verify value 0
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 2); //verify size 2
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[2].first, value)); //remove 2
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[2].second, value); //verify value 2
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 1); //verify size 1
        BOOST_REQUIRE(hm.GetValueAndRemove(HASH, bundleUuidPlusU64Vec[3].first, value)); //remove 3
        BOOST_REQUIRE_EQUAL(bundleUuidPlusU64Vec[3].second, value); //verify value 3
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 0); //verify size 0

        //insert elem 0,2,3 and GetValuePtr 1(fail), 0, 3, 2
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[0].first, bundleUuidPlusU64Vec[0].second)); //insert 0
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[2].first, bundleUuidPlusU64Vec[2].second)); //insert 2
        BOOST_REQUIRE(hm.Insert(HASH, bundleUuidPlusU64Vec[3].first, bundleUuidPlusU64Vec[3].second)); //insert 3
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3
        uint64_t * valuePtr = hm.GetValuePtr(HASH, bundleUuidPlusU64Vec[1].first);
        BOOST_REQUIRE(valuePtr == NULL);
        valuePtr = hm.GetValuePtr(HASH, bundleUuidPlusU64Vec[0].first);
        BOOST_REQUIRE(valuePtr != NULL);
        BOOST_REQUIRE_EQUAL(*valuePtr, bundleUuidPlusU64Vec[0].second);
        valuePtr = hm.GetValuePtr(HASH, bundleUuidPlusU64Vec[3].first);
        BOOST_REQUIRE(valuePtr != NULL);
        BOOST_REQUIRE_EQUAL(*valuePtr, bundleUuidPlusU64Vec[3].second);
        valuePtr = hm.GetValuePtr(HASH, bundleUuidPlusU64Vec[2].first);
        BOOST_REQUIRE(valuePtr != NULL);
        BOOST_REQUIRE_EQUAL(*valuePtr, bundleUuidPlusU64Vec[2].second);
        BOOST_REQUIRE_EQUAL(hm.GetBucketSize(HASH), 3); //verify size 3 still (no deletions)
    }
}
    
BOOST_AUTO_TEST_CASE(BundleUuidToUint64HashMapTestCase)
{
    DoTest<cbhe_bundle_uuid_t>();
    DoTest<cbhe_bundle_uuid_nofragment_t>();
}
