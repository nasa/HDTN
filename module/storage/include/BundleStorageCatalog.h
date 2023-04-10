/**
 * @file BundleStorageCatalog.h
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
 * This BundleStorageCatalog class defines methods for storing key information
 * about bundles in memory, such as bundle expiration, how a bundle is stored/fragmented across disk(s), etc.
 */

#ifndef _BUNDLE_STORAGE_CATALOG_H
#define _BUNDLE_STORAGE_CATALOG_H 1

#include <cstdint>
#include <map>
#include "ForwardListQueue.h"
#include <array>
#include <vector>
#include <utility>
#include <string>
#include "MemoryManagerTreeArray.h"
#include "codec/PrimaryBlock.h"
#include "HashMap16BitFixedSize.h"
#include <boost/bimap.hpp>
#include <boost/date_time.hpp>
#include "CatalogEntry.h"
#include "TelemetryDefinitions.h"
#include "storage_lib_export.h"

//Awaiting Send data structures
typedef ForwardListQueue<uint64_t> custids_flist_queue_t;
typedef std::map<uint64_t, custids_flist_queue_t> expirations_to_custids_map_t;
typedef std::array<expirations_to_custids_map_t, NUMBER_OF_PRIORITIES> priorities_to_expirations_array_t;
typedef std::map<cbhe_eid_t, priorities_to_expirations_array_t> dest_eid_to_priorities_map_t;

typedef HashMap16BitFixedSize<cbhe_bundle_uuid_t, uint64_t> uuid_to_custid_hashmap_t; //get the cteb custody id from fragmented bundle uuid
typedef HashMap16BitFixedSize<cbhe_bundle_uuid_nofragment_t, uint64_t> uuidnofrag_to_custid_hashmap_t; //get the cteb custody id from non-fragmented bundle uuid
typedef HashMap16BitFixedSize<uint64_t, catalog_entry_t> custid_to_catalog_entry_hashmap_t; //get the catalog entry from cteb custody id
typedef boost::bimap<uint64_t, boost::posix_time::ptime> custid_to_custody_xfer_expiry_bimap_t;

class BundleStorageCatalog {
public:
    enum class DUPLICATE_EXPIRY_ORDER {
        FIFO,
        FILO,
        SEQUENCE_NUMBER
    };
    STORAGE_LIB_EXPORT BundleStorageCatalog();
    STORAGE_LIB_EXPORT ~BundleStorageCatalog();

    STORAGE_LIB_EXPORT bool CatalogIncomingBundleForStore(catalog_entry_t & catalogEntryToTake, const PrimaryBlock & primary, const uint64_t custodyId, const DUPLICATE_EXPIRY_ORDER order);

    STORAGE_LIB_EXPORT catalog_entry_t * PopEntryFromAwaitingSend(uint64_t & custodyId, const std::vector<cbhe_eid_t> & availableDestEids);
    STORAGE_LIB_EXPORT catalog_entry_t * PopEntryFromAwaitingSend(uint64_t & custodyId, const std::vector<uint64_t> & availableDestNodeIds);
    STORAGE_LIB_EXPORT catalog_entry_t * PopEntryFromAwaitingSend(uint64_t & custodyId, const std::vector<std::pair<cbhe_eid_t, bool> > & availableDests);
    
    STORAGE_LIB_EXPORT bool AddEntryToAwaitingSend(const catalog_entry_t & catalogEntry, const uint64_t custodyId, const DUPLICATE_EXPIRY_ORDER order);
    STORAGE_LIB_EXPORT bool ReturnEntryToAwaitingSend(const catalog_entry_t & catalogEntry, const uint64_t custodyId);
    STORAGE_LIB_EXPORT bool RemoveEntryFromAwaitingSend(const catalog_entry_t & catalogEntry, const uint64_t custodyId);
    STORAGE_LIB_EXPORT std::pair<bool, uint16_t> Remove(const uint64_t custodyId, bool alsoNeedsRemovedFromAwaitingSend);
    STORAGE_LIB_EXPORT catalog_entry_t * GetEntryFromCustodyId(const uint64_t custodyId);
    STORAGE_LIB_EXPORT uint64_t * GetCustodyIdFromUuid(const cbhe_bundle_uuid_t & bundleUuid);
    STORAGE_LIB_EXPORT uint64_t * GetCustodyIdFromUuid(const cbhe_bundle_uuid_nofragment_t & bundleUuid);
    STORAGE_LIB_EXPORT void GetExpiredBundleIds(const uint64_t expiry, const uint64_t maxNumberToFind, std::vector<uint64_t> & returnedIds);
    STORAGE_LIB_EXPORT bool GetStorageExpiringBeforeThresholdTelemetry(StorageExpiringBeforeThresholdTelemetry_t & telem);

    STORAGE_LIB_EXPORT uint64_t GetNumBundlesInCatalog() const noexcept;
    STORAGE_LIB_EXPORT uint64_t GetNumBundleBytesInCatalog() const noexcept;

    STORAGE_LIB_EXPORT uint64_t GetTotalBundleWriteOperationsToCatalog() const noexcept;
    STORAGE_LIB_EXPORT uint64_t GetTotalBundleByteWriteOperationsToCatalog() const noexcept;

    STORAGE_LIB_EXPORT uint64_t GetTotalBundleEraseOperationsFromCatalog() const noexcept;
    STORAGE_LIB_EXPORT uint64_t GetTotalBundleByteEraseOperationsFromCatalog() const noexcept;

private:
    STORAGE_LIB_NO_EXPORT catalog_entry_t * PopEntryFromAwaitingSend(uint64_t & custodyId,
        const std::vector<std::pair<const cbhe_eid_t*, priorities_to_expirations_array_t *> > & destEidPlusPriorityArrayPtrs);
    STORAGE_LIB_NO_EXPORT bool Insert_OrderBySequence(custids_flist_queue_t& custodyIdFlistQueue, const uint64_t custodyIdToInsert, const uint64_t mySequence);
    STORAGE_LIB_NO_EXPORT void Insert_OrderByFifo(custids_flist_queue_t& custodyIdFlistQueue, const uint64_t custodyIdToInsert);
    STORAGE_LIB_NO_EXPORT void Insert_OrderByFilo(custids_flist_queue_t& custodyIdFlistQueue, const uint64_t custodyIdToInsert);
    STORAGE_LIB_NO_EXPORT bool Remove(custids_flist_queue_t& custodyIdFlistQueue, const uint64_t custodyIdToRemove);

protected:
    dest_eid_to_priorities_map_t m_destEidToPrioritiesMap;
    uuid_to_custid_hashmap_t m_uuidToCustodyIdHashMap;
    uuidnofrag_to_custid_hashmap_t m_uuidNoFragToCustodyIdHashMap;
    custid_to_catalog_entry_hashmap_t m_custodyIdToCatalogEntryHashmap;
    custid_to_custody_xfer_expiry_bimap_t m_custodyIdToCustodyTransferExpiryBimap;
    uint64_t m_numBundlesInCatalog;
    uint64_t m_numBundleBytesInCatalog;
    uint64_t m_totalBundleWriteOperationsToCatalog;
    uint64_t m_totalBundleByteWriteOperationsToCatalog;
    uint64_t m_totalBundleEraseOperationsFromCatalog;
    uint64_t m_totalBundleByteEraseOperationsFromCatalog;
};


#endif //_BUNDLE_STORAGE_CATALOG_H
