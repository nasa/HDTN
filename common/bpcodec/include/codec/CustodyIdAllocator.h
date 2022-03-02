#ifndef CUSTODY_ID_ALLOCATOR_H
#define CUSTODY_ID_ALLOCATOR_H 1


#include <string>
#include <cstdint>
#include "FragmentSet.h"
#include "codec/bpv6.h"
#include <map>
/*
This is a class to help intelligently allocate custody ids with CTEB/ACS.
Custody Ids should be allocated with the smallest number possible, and also
a the bundle sources should have as much contiguous custody ids as possible.
This will minimize CTEB/ACS total bytes transferred when encoded to sdnvs and ACS.

In case of interleaving from multiple bundle sources,
allocate integer range from [N*256+0, N*256+1, ... ,  N*256+255]

*/
class CustodyIdAllocator {
public:
    
    BPCODEC_EXPORT CustodyIdAllocator();
    BPCODEC_EXPORT ~CustodyIdAllocator();

    BPCODEC_EXPORT void InitializeAddUsedCustodyId(const uint64_t custodyId);
    BPCODEC_EXPORT void InitializeAddUsedCustodyIdRange(const uint64_t custodyIdBegin, const uint64_t custodyIdEnd);
    BPCODEC_EXPORT uint64_t FreeCustodyId(const uint64_t custodyId); //return number of multipliers freed
    BPCODEC_EXPORT uint64_t FreeCustodyIdRange(const uint64_t custodyIdBegin, const uint64_t custodyIdEnd); //return number of multipliers freed
    BPCODEC_EXPORT void ReserveNextCustodyIdBlock();
    BPCODEC_EXPORT void Reset();
    BPCODEC_EXPORT uint64_t GetNextCustodyIdForNextHopCtebToSend(const cbhe_eid_t & bundleSrcEid);
    BPCODEC_EXPORT void PrintUsedCustodyIds();
    BPCODEC_EXPORT void PrintUsedCustodyIdMultipliers();
private:

    uint64_t m_myNextCustodyIdAllocationBeginForNextHopCtebToSend;
    std::map<cbhe_eid_t, uint64_t> m_mapBundleSrcEidToNextCtebCustodyId;
    std::set<FragmentSet::data_fragment_t> m_usedCustodyIds;
    std::set<FragmentSet::data_fragment_t> m_usedCustodyIdMultipliers;
};

#endif // CUSTODY_ID_ALLOCATOR_H

