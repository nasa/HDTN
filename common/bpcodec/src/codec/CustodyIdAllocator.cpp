/**
 * @file CustodyIdAllocator.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "codec/CustodyIdAllocator.h"
#include <iostream>



CustodyIdAllocator::CustodyIdAllocator() : m_myNextCustodyIdAllocationBeginForNextHopCtebToSend(UINT64_MAX) {}
CustodyIdAllocator::~CustodyIdAllocator() {}

void CustodyIdAllocator::InitializeAddUsedCustodyId(const uint64_t custodyId) {
    const uint64_t multiplier = custodyId >> 8; //custodyId / 256
    FragmentSet::InsertFragment(m_usedCustodyIdMultipliers, FragmentSet::data_fragment_t(multiplier, multiplier));
    FragmentSet::InsertFragment(m_usedCustodyIds, FragmentSet::data_fragment_t (custodyId, custodyId));
}
void CustodyIdAllocator::InitializeAddUsedCustodyIdRange(const uint64_t custodyIdBegin, const uint64_t custodyIdEnd) {
    const uint64_t multiplierBegin = custodyIdBegin >> 8; //custodyIdBegin / 256
    const uint64_t multiplierEnd = custodyIdEnd >> 8; //custodyIdEnd / 256
    FragmentSet::InsertFragment(m_usedCustodyIdMultipliers, FragmentSet::data_fragment_t(multiplierBegin, multiplierEnd));
    FragmentSet::InsertFragment(m_usedCustodyIds, FragmentSet::data_fragment_t(custodyIdBegin, custodyIdEnd));
}
//return number of multipliers freed
uint64_t CustodyIdAllocator::FreeCustodyId(const uint64_t custodyId) {
    FragmentSet::RemoveFragment(m_usedCustodyIds, FragmentSet::data_fragment_t(custodyId, custodyId));
    const uint64_t multiplier = custodyId >> 8; //custodyId / 256
    const uint64_t blockBase = multiplier << 8; //multiplier * 256
    const FragmentSet::data_fragment_t blockOfCustodyIds(blockBase, blockBase + 255);
    if (FragmentSet::DoesNotContainFragmentEntirely(m_usedCustodyIds, blockOfCustodyIds)) {
        //blockOfCustodyIds not contained/set inside of m_usedCustodyIdMultipliers (may still abut)
        const FragmentSet::data_fragment_t multiplierToRemove(multiplier, multiplier);
        if (FragmentSet::ContainsFragmentEntirely(m_usedCustodyIdMultipliers, multiplierToRemove)) {
            FragmentSet::RemoveFragment(m_usedCustodyIdMultipliers, multiplierToRemove);
            return 1;
        }
    }
    return 0;
}
//return number of multipliers freed
uint64_t CustodyIdAllocator::FreeCustodyIdRange(const uint64_t custodyIdBegin, const uint64_t custodyIdEnd) {
    FragmentSet::RemoveFragment(m_usedCustodyIds, FragmentSet::data_fragment_t(custodyIdBegin, custodyIdEnd));
    const uint64_t multiplierBegin = custodyIdBegin >> 8; //custodyIdBegin / 256
    const uint64_t multiplierEnd = custodyIdEnd >> 8; //custodyIdEnd / 256
    uint64_t numMultipliersRemoved = 0;
    for (uint64_t multiplier = multiplierBegin; multiplier <= multiplierEnd; ++multiplier) {
        const uint64_t blockBase = multiplier << 8; //multiplier * 256
        const FragmentSet::data_fragment_t blockOfCustodyIds(blockBase, blockBase + 255);
        if (FragmentSet::DoesNotContainFragmentEntirely(m_usedCustodyIds, blockOfCustodyIds)) {
            //blockOfCustodyIds not contained/set inside of m_usedCustodyIdMultipliers (may still abut)
            const FragmentSet::data_fragment_t multiplierToRemove(multiplier, multiplier);
            if (FragmentSet::ContainsFragmentEntirely(m_usedCustodyIdMultipliers, multiplierToRemove)) {
                FragmentSet::RemoveFragment(m_usedCustodyIdMultipliers, multiplierToRemove);
                ++numMultipliersRemoved;
            }
        }
    }
    return numMultipliersRemoved;

}


//sets m_myNextCustodyIdAllocationBeginForNextHopCtebToSend
void CustodyIdAllocator::ReserveNextCustodyIdBlock() {
    uint64_t multiplier;
    if (m_usedCustodyIdMultipliers.empty()) { //if empty start at 0
        multiplier = 0;
    }
    else { //pick the lowest value from begin() iterator
        const FragmentSet::data_fragment_t & multFrag = *(m_usedCustodyIdMultipliers.cbegin());
        multiplier = (multFrag.beginIndex != 0) ? 0 : (multFrag.endIndex + 1);
    }
    

    //use selected multiplier to update the sets
    FragmentSet::InsertFragment(m_usedCustodyIdMultipliers, FragmentSet::data_fragment_t(multiplier, multiplier));
    const uint64_t blockBase = multiplier << 8; //multiplier * 256
    const FragmentSet::data_fragment_t blockOfCustodyIds(blockBase, blockBase + 255);
    FragmentSet::InsertFragment(m_usedCustodyIds, blockOfCustodyIds);
    m_myNextCustodyIdAllocationBeginForNextHopCtebToSend = blockBase;
}
void CustodyIdAllocator::Reset() {

}

//bundle sources should have as much contiguous custody ids as possible
//in case of interleaving from multiple bundle sources,
//allocate integer range from [N*256+0, N*256+1, ... ,  N*256+255]
uint64_t CustodyIdAllocator::GetNextCustodyIdForNextHopCtebToSend(const cbhe_eid_t & bundleSrcEid) {
    if (m_myNextCustodyIdAllocationBeginForNextHopCtebToSend == UINT64_MAX) {
        ReserveNextCustodyIdBlock(); //set m_myNextCustodyIdAllocationBeginForNextHopCtebToSend
    }
    std::pair<std::map<cbhe_eid_t, uint64_t>::iterator, bool> res = m_mapBundleSrcEidToNextCtebCustodyId.insert(
        std::pair<cbhe_eid_t, uint64_t>(bundleSrcEid, m_myNextCustodyIdAllocationBeginForNextHopCtebToSend + 1)); //+1 because it's the next
    if (res.second == true) { //insertion due to first bundleSrcEid
        const uint64_t retVal = m_myNextCustodyIdAllocationBeginForNextHopCtebToSend;
        ReserveNextCustodyIdBlock(); //set m_myNextCustodyIdAllocationBeginForNextHopCtebToSend  (usually += 256);
        return retVal;
    }
    else {
        //found but not inserted
        uint64_t & nextCtebCustodyId = res.first->second;
        const uint64_t retVal = nextCtebCustodyId;
        if ((++nextCtebCustodyId & 0xff) == 0) {
            nextCtebCustodyId = m_myNextCustodyIdAllocationBeginForNextHopCtebToSend;
            ReserveNextCustodyIdBlock(); //set m_myNextCustodyIdAllocationBeginForNextHopCtebToSend  (usually += 256);
        }
        return retVal;
    }
}

void CustodyIdAllocator::PrintUsedCustodyIds() {
    FragmentSet::PrintFragmentSet(m_usedCustodyIds);
}
void CustodyIdAllocator::PrintUsedCustodyIdMultipliers() {
    FragmentSet::PrintFragmentSet(m_usedCustodyIdMultipliers);
}
