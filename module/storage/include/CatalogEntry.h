/**
 * @file CatalogEntry.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This CatalogEntry class defines the data structures for storing key information
 * about bundles in memory and is used by the BundleStorageCatalog class.
 */

#ifndef _CATALOG_ENTRY_H
#define _CATALOG_ENTRY_H 1

#include <cstdint>
#include "MemoryManagerTreeArray.h"
#include "codec/PrimaryBlock.h"
#include "storage_lib_export.h"

struct catalog_entry_t {
    uint64_t bundleSizeBytes;
    uint64_t payloadSizeBytes;
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
    STORAGE_LIB_EXPORT void Init(const PrimaryBlock & primary, const uint64_t paramBundleSizeBytes, const uint64_t paramPayloadSizeBytes, const uint64_t paramNumSegmentsRequired, void * paramPtrUuidKeyInMap, cbhe_eid_t *bundleEidMaskPtr = NULL);
};

#endif //_CATALOG_ENTRY_H
