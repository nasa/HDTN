#ifndef _BUNDLE_STORAGE_CATALOG_H
#define _BUNDLE_STORAGE_CATALOG_H

#include <cstdint>
#include <map>
#include <forward_list>
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

//Awaiting Send data structures
typedef std::forward_list<uint64_t> custids_flist_t;
typedef std::pair<custids_flist_t, custids_flist_t::iterator> custids_flist_plus_lastiterator_t;
typedef std::map<uint64_t, custids_flist_plus_lastiterator_t> expirations_to_custids_map_t;
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
    BundleStorageCatalog();
    ~BundleStorageCatalog();

    bool CatalogIncomingBundleForStore(catalog_entry_t & catalogEntryToTake, const PrimaryBlock & primary, const uint64_t custodyId, const DUPLICATE_EXPIRY_ORDER order);

    catalog_entry_t * PopEntryFromAwaitingSend(uint64_t & custodyId, const std::vector<cbhe_eid_t> & availableDestEids);
    catalog_entry_t * PopEntryFromAwaitingSend(uint64_t & custodyId, const std::vector<uint64_t> & availableDestNodeIds);
    catalog_entry_t * PopEntryFromAwaitingSend(uint64_t & custodyId, const std::vector<std::pair<cbhe_eid_t, bool> > & availableDests);
    
    bool AddEntryToAwaitingSend(const catalog_entry_t & catalogEntry, const uint64_t custodyId, const DUPLICATE_EXPIRY_ORDER order);
    bool ReturnEntryToAwaitingSend(const catalog_entry_t & catalogEntry, const uint64_t custodyId);
    bool RemoveEntryFromAwaitingSend(const catalog_entry_t & catalogEntry, const uint64_t custodyId);
    std::pair<bool, uint16_t> Remove(const uint64_t custodyId, bool alsoNeedsRemovedFromAwaitingSend);
    catalog_entry_t * GetEntryFromCustodyId(const uint64_t custodyId);
    uint64_t * GetCustodyIdFromUuid(const cbhe_bundle_uuid_t & bundleUuid);
    uint64_t * GetCustodyIdFromUuid(const cbhe_bundle_uuid_nofragment_t & bundleUuid);

private:
    catalog_entry_t * PopEntryFromAwaitingSend(uint64_t & custodyId,
        const std::vector<std::pair<const cbhe_eid_t*, priorities_to_expirations_array_t *> > & destEidPlusPriorityArrayPtrs);
    bool Insert_OrderBySequence(custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt, const uint64_t custodyIdToInsert, const uint64_t mySequence);
    void Insert_OrderByFifo(custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt, const uint64_t custodyIdToInsert);
    void Insert_OrderByFilo(custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt, const uint64_t custodyIdToInsert);
    bool Remove(custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt, const uint64_t custodyIdToRemove);

protected:
    dest_eid_to_priorities_map_t m_destEidToPrioritiesMap;
    uuid_to_custid_hashmap_t m_uuidToCustodyIdHashMap;
    uuidnofrag_to_custid_hashmap_t m_uuidNoFragToCustodyIdHashMap;
    custid_to_catalog_entry_hashmap_t m_custodyIdToCatalogEntryHashmap;
    custid_to_custody_xfer_expiry_bimap_t m_custodyIdToCustodyTransferExpiryBimap;
};


#endif //_BUNDLE_STORAGE_CATALOG_H
