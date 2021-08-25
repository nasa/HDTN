/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "BundleStorageCatalog.h"
#include <iostream>
#include <string>
#include <boost/make_unique.hpp>


BundleStorageCatalog::BundleStorageCatalog() {}



BundleStorageCatalog::~BundleStorageCatalog() {}

bool BundleStorageCatalog::Insert_OrderBySequence(custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt, const uint64_t custodyIdToInsert, const uint64_t mySequence) {
    custids_flist_t & custodyIdFlist = custodyIdFlistPlusLastIt.first;
    custids_flist_t::iterator & custodyIdFlistLastIterator = custodyIdFlistPlusLastIt.second;
    if (custodyIdFlist.empty()) {
        custodyIdFlist.emplace_front(custodyIdToInsert);
        custodyIdFlistLastIterator = custodyIdFlist.begin();
        return true;
    }
    else {
        
        //Chances are that bundle sequences will arrive in numerical order.
        //To optimize ordered insertion into an ordered singly-linked list, check if greater than the
        //last iterator first before iterating through the list.
        const uint64_t lastCustodyIdInList = *custodyIdFlistLastIterator;
        catalog_entry_t * lastEntryInList = m_custodyIdToCatalogEntryHashmap.GetValuePtr(lastCustodyIdInList);
        if (lastEntryInList == NULL) {
            return false; //unexpected error
        }
        const uint64_t lastSequenceInList = lastEntryInList->sequence;
        if (lastSequenceInList < mySequence) {
            //sequence is greater than every element in the bucket, insert at the end
            custodyIdFlistLastIterator = custodyIdFlist.emplace_after(custodyIdFlistLastIterator, custodyIdToInsert);
            return true;
        }
        

        //An out of order sequence arrived.  Revert to classic insertion
        custids_flist_t::const_iterator itPrev = custodyIdFlist.cbefore_begin();
        for (custids_flist_t::const_iterator it = custodyIdFlist.cbegin(); it != custodyIdFlist.cend(); ++it, ++itPrev) {
            const uint64_t custodyIdInList = *it;
            catalog_entry_t * entryInList = m_custodyIdToCatalogEntryHashmap.GetValuePtr(custodyIdInList);
            if (entryInList == NULL) {
                return false; //unexpected error
            }
            const uint64_t sequenceInList = entryInList->sequence;
            if (mySequence < sequenceInList) { //not in list, insert now
                break; //will call bucket.emplace_after(itPrev, custodyIdToInsert); and return true;
            }
            else if (sequenceInList < mySequence) { //greater than element
                continue;
            }
            else { //equal, already exists
                return false;
            }
        }
        //sequence is greater than every element iterated thus far in the bucket (but less than the last element), insert here
        custodyIdFlist.emplace_after(itPrev, custodyIdToInsert);
        return true;
    }
}
//won't detect duplicates
void BundleStorageCatalog::Insert_OrderByFifo(custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt, const uint64_t custodyIdToInsert) {
    custids_flist_t & custodyIdFlist = custodyIdFlistPlusLastIt.first;
    custids_flist_t::iterator & custodyIdFlistLastIterator = custodyIdFlistPlusLastIt.second;
    if (custodyIdFlist.empty()) {
        custodyIdFlist.emplace_front(custodyIdToInsert);
        custodyIdFlistLastIterator = custodyIdFlist.begin();
    }
    else {
        custodyIdFlistLastIterator = custodyIdFlist.emplace_after(custodyIdFlistLastIterator, custodyIdToInsert);
    }
}
//won't detect duplicates
void BundleStorageCatalog::Insert_OrderByFilo(custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt, const uint64_t custodyIdToInsert) {
    custids_flist_t & custodyIdFlist = custodyIdFlistPlusLastIt.first;
    custids_flist_t::iterator & custodyIdFlistLastIterator = custodyIdFlistPlusLastIt.second;
    const bool startedAsEmpty = custodyIdFlist.empty();
    custodyIdFlist.emplace_front(custodyIdToInsert);
    if (startedAsEmpty) {
        custodyIdFlistLastIterator = custodyIdFlist.begin();
    }
}

bool BundleStorageCatalog::Remove(custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt, const uint64_t custodyIdToRemove) {
    custids_flist_t & custodyIdFlist = custodyIdFlistPlusLastIt.first;
    custids_flist_t::iterator & custodyIdFlistLastIterator = custodyIdFlistPlusLastIt.second;
    if (custodyIdFlist.empty()) {
        return false;
    }
    else {
        for (custids_flist_t::iterator itPrev = custodyIdFlist.before_begin(), it = custodyIdFlist.begin(); it != custodyIdFlist.end(); ++it, ++itPrev) {
            const uint64_t custodyIdInList = *it;
            if (custodyIdToRemove == custodyIdInList) { //equal, remove it
                if (it == custodyIdFlistLastIterator) {
                    custodyIdFlistLastIterator = itPrev;
                }
                custodyIdFlist.erase_after(itPrev);
                return true;
            }
        }
        //not found
        return false;
    }
}

bool BundleStorageCatalog::CatalogIncomingBundleForStore(catalog_entry_t & catalogEntryToTake, const bpv6_primary_block & primary, const uint64_t custodyId, const DUPLICATE_EXPIRY_ORDER order) {
    if (!AddEntryToAwaitingSend(catalogEntryToTake, custodyId, order)) {
        return false;
    }
    if (!m_custodyIdToCatalogEntryHashmap.Insert(custodyId, std::move(catalogEntryToTake))) {
        return false;
    }
    const uint32_t flags = bpv6_bundle_get_gflags(primary.flags);
    if (flags & BPV6_BUNDLEFLAG_CUSTODY) {
        if (flags & BPV6_BUNDLEFLAG_FRAGMENT) {
            return m_uuidToCustodyIdHashMap.Insert(cbhe_bundle_uuid_t(primary), custodyId);
        }
        else {
            return m_uuidNoFragToCustodyIdHashMap.Insert(cbhe_bundle_uuid_nofragment_t(primary), custodyId);
        }
    }
    return true;
}
bool BundleStorageCatalog::AddEntryToAwaitingSend(const catalog_entry_t & catalogEntry, const uint64_t custodyId, const DUPLICATE_EXPIRY_ORDER order) {
    priorities_to_expirations_array_t & priorityArray = m_destEidToPrioritiesMap[catalogEntry.destEid]; //created if not exist
    expirations_to_custids_map_t & expirationMap = priorityArray[catalogEntry.GetPriorityIndex()];
    custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt = expirationMap[catalogEntry.GetAbsExpiration()];
    if (order == DUPLICATE_EXPIRY_ORDER::SEQUENCE_NUMBER) {
        return Insert_OrderBySequence(custodyIdFlistPlusLastIt, custodyId, catalogEntry.sequence);
    }
    else if (order == DUPLICATE_EXPIRY_ORDER::FIFO) {
        Insert_OrderByFifo(custodyIdFlistPlusLastIt, custodyId);
        return true;
    }
    else if (order == DUPLICATE_EXPIRY_ORDER::FILO) {
        Insert_OrderByFilo(custodyIdFlistPlusLastIt, custodyId);
        return true;
    }
    else {
        return false;
    }
}
bool BundleStorageCatalog::ReturnEntryToAwaitingSend(const catalog_entry_t & catalogEntry, const uint64_t custodyId) {
    //return what was popped off the front back to the front
    return AddEntryToAwaitingSend(catalogEntry, custodyId, DUPLICATE_EXPIRY_ORDER::FILO); 
}
bool BundleStorageCatalog::RemoveEntryFromAwaitingSend(const catalog_entry_t & catalogEntry, const uint64_t custodyId) {
    dest_eid_to_priorities_map_t::iterator destEidIt = m_destEidToPrioritiesMap.find(catalogEntry.destEid);
    if (destEidIt != m_destEidToPrioritiesMap.end()) {
        priorities_to_expirations_array_t & priorityArray = destEidIt->second; //created if not exist
        expirations_to_custids_map_t & expirationMap = priorityArray[catalogEntry.GetPriorityIndex()];
        expirations_to_custids_map_t::iterator expirationsIt = expirationMap.find(catalogEntry.GetAbsExpiration());
        if (expirationsIt != expirationMap.end()) {
            custids_flist_plus_lastiterator_t & custodyIdFlistPlusLastIt = expirationsIt->second;
            return Remove(custodyIdFlistPlusLastIt, custodyId);
        }
    }
    return false;
}
catalog_entry_t * BundleStorageCatalog::PopEntryFromAwaitingSend(uint64_t & custodyId, const std::vector<cbhe_eid_t> & availableDestEids) {
    std::vector<std::pair<const cbhe_eid_t*, priorities_to_expirations_array_t *> > destEidPlusPriorityArrayPtrs;
    destEidPlusPriorityArrayPtrs.reserve(availableDestEids.size());

    for (std::size_t i = 0; i < availableDestEids.size(); ++i) {
        const cbhe_eid_t & currentAvailableLink = availableDestEids[i];
        dest_eid_to_priorities_map_t::iterator dmIt = m_destEidToPrioritiesMap.find(currentAvailableLink);
        if (dmIt != m_destEidToPrioritiesMap.end()) {
            destEidPlusPriorityArrayPtrs.emplace_back(&currentAvailableLink, &(dmIt->second));
        }
    }

    //memset((uint8_t*)session.readCacheIsSegmentReady, 0, READ_CACHE_NUM_SEGMENTS_PER_SESSION);
    for (int i = NUMBER_OF_PRIORITIES - 1; i >= 0; --i) { //00 = bulk, 01 = normal, 10 = expedited
        uint64_t lowestExpiration = UINT64_MAX;
        expirations_to_custids_map_t * expirationMapPtr = NULL;
        custids_flist_plus_lastiterator_t * custIdFlistPlusLastItPtr = NULL;
        expirations_to_custids_map_t::iterator expirationMapIterator;

        for (std::size_t j = 0; j < destEidPlusPriorityArrayPtrs.size(); ++j) {
            priorities_to_expirations_array_t * priorityArray = destEidPlusPriorityArrayPtrs[j].second;
            //std::cout << "size " << (*priorityVec).size() << "\n";
            expirations_to_custids_map_t & expirationMap = (*priorityArray)[i];
            expirations_to_custids_map_t::iterator it = expirationMap.begin();
            if (it != expirationMap.end()) {
                const uint64_t thisExpiration = it->first;
                //std::cout << "thisexp " << thisExpiration << "\n";
                if (lowestExpiration > thisExpiration) {
                    lowestExpiration = thisExpiration;
                    expirationMapPtr = &expirationMap;
                    custIdFlistPlusLastItPtr = &it->second;
                    expirationMapIterator = it;
                }
            }
        }
        if (custIdFlistPlusLastItPtr) {
            custids_flist_t & cidFlist = custIdFlistPlusLastItPtr->first;
            custodyId = cidFlist.front();
            cidFlist.pop_front();

            if (cidFlist.empty()) {
                expirationMapPtr->erase(expirationMapIterator);
            }

            return m_custodyIdToCatalogEntryHashmap.GetValuePtr(custodyId);
        }
    }
    return NULL;
}

bool BundleStorageCatalog::StartCustodyTransferTimer(const uint64_t custodyId, const boost::posix_time::time_duration & timeout) {
    boost::posix_time::ptime expiry;
    do { //loop to make sure expiry is unique in case multiple calls to StartTimer are too close
        expiry = boost::posix_time::microsec_clock::universal_time() + timeout;
    } while (m_custodyIdToCustodyTransferExpiryBimap.right.count(expiry));

    return m_custodyIdToCustodyTransferExpiryBimap.left.insert(custid_to_custody_xfer_expiry_bimap_t::left_value_type(custodyId, expiry)).second;
}
bool BundleStorageCatalog::CancelCustodyTransferTimer(const uint64_t custodyId) {
    custid_to_custody_xfer_expiry_bimap_t::left_iterator it = m_custodyIdToCustodyTransferExpiryBimap.left.find(custodyId);
    if (it != m_custodyIdToCustodyTransferExpiryBimap.left.end()) {
        m_custodyIdToCustodyTransferExpiryBimap.left.erase(it);
        return true;
    }
    else {
        return false;
    }
}
bool BundleStorageCatalog::CancelCustodyTransferTimerAtLowestExpiry(const uint64_t expectedCustodyIdToCancel) {
    custid_to_custody_xfer_expiry_bimap_t::right_iterator it = m_custodyIdToCustodyTransferExpiryBimap.right.begin();
    if ((it != m_custodyIdToCustodyTransferExpiryBimap.right.end()) && (it->second == expectedCustodyIdToCancel)) {
        m_custodyIdToCustodyTransferExpiryBimap.right.erase(it);
        return true;
    }
    else {
        return false;
    }
}
bool BundleStorageCatalog::HasExpiredCustodyTimer(uint64_t & custodyId) {
    custid_to_custody_xfer_expiry_bimap_t::right_iterator it = m_custodyIdToCustodyTransferExpiryBimap.right.begin();
    if (it != m_custodyIdToCustodyTransferExpiryBimap.right.end()) {
        if (it->first < boost::posix_time::microsec_clock::universal_time()) { //expired
            custodyId = it->second;
            return true;
        }
    }
    return false;
}
std::size_t BundleStorageCatalog::GetNumCustodyTransferTimers() {
    return m_custodyIdToCustodyTransferExpiryBimap.size();
}


//return pair<success, numSuccessfulRemovals>
std::pair<bool, uint16_t> BundleStorageCatalog::Remove(const uint64_t custodyId, const bpv6_primary_block & primary, bool alsoNeedsRemovedFromAwaitingSend) {
    catalog_entry_t entry;
    bool error = false;
    uint16_t numRemovals = 0;
    if (!m_custodyIdToCatalogEntryHashmap.GetValueAndRemove(custodyId, entry)) {
        error = true;
    }
    else {
        ++numRemovals;
    }
    if ((!error) && alsoNeedsRemovedFromAwaitingSend) {
        if (!RemoveEntryFromAwaitingSend(entry, custodyId)) {
            error = true;
        }
        else {
            ++numRemovals;
        }
    }
    const uint32_t flags = bpv6_bundle_get_gflags(primary.flags);
    if (flags & BPV6_BUNDLEFLAG_CUSTODY) {
        uint64_t cidInMap;
        if (flags & BPV6_BUNDLEFLAG_FRAGMENT) {
            if (!m_uuidToCustodyIdHashMap.GetValueAndRemove(cbhe_bundle_uuid_t(primary), cidInMap)) {
                error = true;
            }
            else {
                ++numRemovals;
            }
        }
        else {
            if (!m_uuidNoFragToCustodyIdHashMap.GetValueAndRemove(cbhe_bundle_uuid_nofragment_t(primary), cidInMap)) {
                error = true;
            }
            else {
                ++numRemovals;
            }
        }
        if (cidInMap != custodyId) {
            error = true;
        }
    }
    return std::pair<bool, uint16_t>(!error, numRemovals);
}
catalog_entry_t * BundleStorageCatalog::GetEntryFromCustodyId(const uint64_t custodyId) {
    return m_custodyIdToCatalogEntryHashmap.GetValuePtr(custodyId);
}
uint64_t * BundleStorageCatalog::GetCustodyIdFromUuid(const cbhe_bundle_uuid_t & bundleUuid) {
    return m_uuidToCustodyIdHashMap.GetValuePtr(bundleUuid);
}
uint64_t * BundleStorageCatalog::GetCustodyIdFromUuid(const cbhe_bundle_uuid_nofragment_t & bundleUuid) {
    return m_uuidNoFragToCustodyIdHashMap.GetValuePtr(bundleUuid);
}
#if 0




bool BundleStorageManagerBase::ReturnTop(BundleStorageManagerSession_ReadFromDisk & session) { //0 if empty, size if entry
    (*session.expirationMapPtr)[session.absExpiration].push_front(std::move(session.chainInfo));
    return true;
}

std::size_t BundleStorageManagerBase::TopSegment(BundleStorageManagerSession_ReadFromDisk & session, void * buf) {
    segment_id_chain_vec_t & segments = session.chainInfo.second;

    while (((session.nextLogicalSegmentToCache - session.nextLogicalSegment) < READ_CACHE_NUM_SEGMENTS_PER_SESSION)
        && (session.nextLogicalSegmentToCache < segments.size()))
    {
        const segment_id_t segmentId = segments[session.nextLogicalSegmentToCache++];
        const unsigned int diskIndex = segmentId % M_NUM_STORAGE_DISKS;
        CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb = m_circularIndexBuffersVec[diskIndex];
        unsigned int produceIndex = cb.GetIndexForWrite();
        while (produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
            m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
            produceIndex = cb.GetIndexForWrite();
        }

        session.readCacheIsSegmentReady[session.cacheWriteIndex] = false;
        m_circularBufferIsReadCompletedPointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = &session.readCacheIsSegmentReady[session.cacheWriteIndex];
        m_circularBufferReadFromStoragePointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = &session.readCache[session.cacheWriteIndex * SEGMENT_SIZE];
        session.cacheWriteIndex = (session.cacheWriteIndex + 1) % READ_CACHE_NUM_SEGMENTS_PER_SESSION;
        m_circularBufferSegmentIdsPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = segmentId;

        cb.CommitWrite();
        NotifyDiskOfWorkToDo_ThreadSafe(diskIndex);
    }

    bool readIsReady = session.readCacheIsSegmentReady[session.cacheReadIndex];
    while (!readIsReady) { //store the volatile, wait until not full				
        m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
        readIsReady = session.readCacheIsSegmentReady[session.cacheReadIndex];
    }

    boost::uint64_t bundleSizeBytes;
    memcpy(&bundleSizeBytes, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + 0], sizeof(bundleSizeBytes));
    if (session.nextLogicalSegment == 0 && bundleSizeBytes != session.chainInfo.first) {// ? chainInfo.first : UINT64_MAX;
        std::cout << "error: read bundle size bytes = " << bundleSizeBytes << " does not match chainInfo = " << session.chainInfo.first << "\n";
        hdtn::Logger::getInstance()->logError("storage", "Error: read bundle size bytes = " + std::to_string(bundleSizeBytes) +
            " does not match chainInfo = " + std::to_string(session.chainInfo.first));
    }
    else if (session.nextLogicalSegment != 0 && bundleSizeBytes != UINT64_MAX) {// ? chainInfo.first : UINT64_MAX;
        std::cout << "error: read bundle size bytes = " << bundleSizeBytes << " is not UINT64_MAX\n";
        hdtn::Logger::getInstance()->logError("storage", "Error: read bundle size bytes = " + std::to_string(bundleSizeBytes) +
            " is not UINT64_MAX");
    }

    segment_id_t nextSegmentId;
    ++session.nextLogicalSegment;
    memcpy(&nextSegmentId, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + sizeof(bundleSizeBytes)], sizeof(nextSegmentId));
    if (session.nextLogicalSegment != session.chainInfo.second.size() && nextSegmentId != session.chainInfo.second[session.nextLogicalSegment]) {
        std::cout << "error: read nextSegmentId = " << nextSegmentId << " does not match chainInfo = " << session.chainInfo.second[session.nextLogicalSegment] << "\n";
        hdtn::Logger::getInstance()->logError("storage", "Error: read nextSegmentId = " + std::to_string(nextSegmentId) +
            " does not match chainInfo = " + std::to_string(session.chainInfo.second[session.nextLogicalSegment]));
    }
    else if (session.nextLogicalSegment == session.chainInfo.second.size() && nextSegmentId != UINT32_MAX) {
        std::cout << "error: read nextSegmentId = " << nextSegmentId << " is not UINT32_MAX\n";
        hdtn::Logger::getInstance()->logError("storage", "Error: read nextSegmentId = " + std::to_string(nextSegmentId) +
            " is not UINT32_MAX");
    }

    std::size_t size = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
    if (nextSegmentId == UINT32_MAX) {
        boost::uint64_t modBytes = (session.chainInfo.first % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
        if (modBytes != 0) {
            size = modBytes;
        }
    }

    memcpy(buf, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + SEGMENT_RESERVED_SPACE], size);
    session.cacheReadIndex = (session.cacheReadIndex + 1) % READ_CACHE_NUM_SEGMENTS_PER_SESSION;


    return size;
}
bool BundleStorageManagerBase::RemoveReadBundleFromDisk(BundleStorageManagerSession_ReadFromDisk & session, bool forceRemove) {
    if (!forceRemove && (session.nextLogicalSegment != session.chainInfo.second.size())) {
        std::cout << "error: bundle not yet read prior to removal\n";
        hdtn::Logger::getInstance()->logError("storage", "Error: bundle not yet read prior to removal");
        return false;
    }


    //destroy the head on the disk by writing UINT64_MAX to bundleSizeBytes of first logical segment
    chain_info_t & chainInfo = session.chainInfo;
    segment_id_chain_vec_t & segmentIdChainVec = chainInfo.second;


    const boost::uint64_t bundleSizeBytes = UINT64_MAX;
    const segment_id_t segmentId = segmentIdChainVec[0];
    const unsigned int diskIndex = segmentId % M_NUM_STORAGE_DISKS;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb = m_circularIndexBuffersVec[diskIndex];
    unsigned int produceIndex = cb.GetIndexForWrite();
    while (produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
        m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
        produceIndex = cb.GetIndexForWrite();
    }

    boost::uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
    segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE];


    boost::uint8_t * const dataCb = &circularBufferBlockDataPtr[produceIndex * SEGMENT_SIZE];
    circularBufferSegmentIdsPtr[produceIndex] = segmentId;
    m_circularBufferReadFromStoragePointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = NULL; //isWriteToDisk = true

    memcpy(dataCb, &bundleSizeBytes, sizeof(bundleSizeBytes));


    cb.CommitWrite();
    NotifyDiskOfWorkToDo_ThreadSafe(diskIndex);

    return m_memoryManager.FreeSegments_ThreadSafe(segmentIdChainVec);
}
#endif
