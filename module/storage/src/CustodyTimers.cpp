/**
 * @file CustodyTimers.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "CustodyTimers.h"
#include <string>
#include <boost/make_unique.hpp>


CustodyTimers::CustodyTimers(const boost::posix_time::time_duration & timeout) : M_CUSTODY_TIMEOUT_DURATION(timeout) {}



CustodyTimers::~CustodyTimers() {}


bool CustodyTimers::PollOneAndPopExpiredCustodyTimer(uint64_t & custodyId, const std::vector<cbhe_eid_t> & availableDestEids, const boost::posix_time::ptime & nowPtime) {
    boost::posix_time::ptime lowestExpiry = boost::posix_time::special_values::pos_infin;
    desteid_to_custidexpirylist_map_t::iterator lowestExpiryIt;
    for (std::size_t i = 0; i < availableDestEids.size(); ++i) {
        const cbhe_eid_t & currentAvailableLink = availableDestEids[i];
        desteid_to_custidexpirylist_map_t::iterator it = m_mapDestEidToCustodyIdExpiryList.find(currentAvailableLink);
        if (it != m_mapDestEidToCustodyIdExpiryList.end()) {
            custid_ptime_list_t & custIdPlusPtimeList = it->second;
            if (!custIdPlusPtimeList.empty()) {
                custid_ptime_pair_t & thisCustodyIdPlusPtimePair = custIdPlusPtimeList.front();
                boost::posix_time::ptime & thisExpiry = thisCustodyIdPlusPtimePair.second;
                if (lowestExpiry > thisExpiry) {
                    lowestExpiry = thisExpiry;
                    lowestExpiryIt = it;
                }
            }
        }
    }
    if (lowestExpiry <= nowPtime) {
        custid_ptime_list_t & custIdPlusPtimeList = lowestExpiryIt->second;
        const custid_ptime_pair_t & thisCustodyIdPlusPtimePair = custIdPlusPtimeList.front();
        custodyId = thisCustodyIdPlusPtimePair.first;
        custIdPlusPtimeList.pop_front();
        if (custIdPlusPtimeList.empty()) {
            m_mapDestEidToCustodyIdExpiryList.erase(lowestExpiryIt);
        }
        return (m_mapCustodyIdToListIterator.erase(custodyId) == 1);
    }
    return false;
}

bool CustodyTimers::PollOneAndPopAnyExpiredCustodyTimer(uint64_t & custodyId, const boost::posix_time::ptime & nowPtime) {
    for (desteid_to_custidexpirylist_map_t::iterator it = m_mapDestEidToCustodyIdExpiryList.begin(); it != m_mapDestEidToCustodyIdExpiryList.end(); ++it) {
        custid_ptime_list_t & custIdPlusPtimeList = it->second;
        if (!custIdPlusPtimeList.empty()) {
            custid_ptime_pair_t & thisCustodyIdPlusPtimePair = custIdPlusPtimeList.front();
            boost::posix_time::ptime & thisExpiry = thisCustodyIdPlusPtimePair.second;
            if (thisExpiry <= nowPtime) {
                custid_ptime_list_t & custIdPlusPtimeList = it->second;
                const custid_ptime_pair_t & thisCustodyIdPlusPtimePair = custIdPlusPtimeList.front();
                custodyId = thisCustodyIdPlusPtimePair.first;
                custIdPlusPtimeList.pop_front();
                if (custIdPlusPtimeList.empty()) {
                    m_mapDestEidToCustodyIdExpiryList.erase(it);
                }
                return (m_mapCustodyIdToListIterator.erase(custodyId) == 1);
            }
        }
    }
    return false;
}

bool CustodyTimers::StartCustodyTransferTimer(const cbhe_eid_t & finalDestEid, const uint64_t custodyId) {
    //expiry will always be appended to list (always greater than previous) (duplicate expiries ok)
    const boost::posix_time::ptime expiry = boost::posix_time::microsec_clock::universal_time() + M_CUSTODY_TIMEOUT_DURATION;

    typename std::pair<custid_to_listiterator_map_t::iterator, bool> retVal =
        m_mapCustodyIdToListIterator.insert(
            custid_to_listiterator_map_insertion_element_t(custodyId, custid_ptime_list_t::iterator()));
    if (retVal.second) {
        //value was inserted
        custid_ptime_list_t & listRef = m_mapDestEidToCustodyIdExpiryList[finalDestEid];
        retVal.first->second = listRef.insert(listRef.end(), custid_ptime_pair_t(custodyId, expiry));
        return true;
    }
    return false;
}
bool CustodyTimers::CancelCustodyTransferTimer(const cbhe_eid_t & finalDestEid, const uint64_t custodyId) {
    custid_to_listiterator_map_t::iterator itCidToListItMap = m_mapCustodyIdToListIterator.find(custodyId);
    if (itCidToListItMap != m_mapCustodyIdToListIterator.end()) {
        desteid_to_custidexpirylist_map_t::iterator itEidToCidExpiryListMap = m_mapDestEidToCustodyIdExpiryList.find(finalDestEid);
        if (itEidToCidExpiryListMap != m_mapDestEidToCustodyIdExpiryList.end()) {
            custid_ptime_list_t & listRef = itEidToCidExpiryListMap->second;
            listRef.erase(itCidToListItMap->second);
            if (listRef.empty()) {
                m_mapDestEidToCustodyIdExpiryList.erase(itEidToCidExpiryListMap);
            }
            m_mapCustodyIdToListIterator.erase(itCidToListItMap);
            return true;
        }
        return false;
    }
    return false;
}

std::size_t CustodyTimers::GetNumCustodyTransferTimers() {
    std::size_t sum = 0;
    for (desteid_to_custidexpirylist_map_t::iterator it = m_mapDestEidToCustodyIdExpiryList.begin(); it != m_mapDestEidToCustodyIdExpiryList.end(); ++it) {
        sum += it->second.size();
    }
    return sum;
}

std::size_t CustodyTimers::GetNumCustodyTransferTimers(const cbhe_eid_t & finalDestEid) {
    desteid_to_custidexpirylist_map_t::iterator itEidToCidExpiryListMap = m_mapDestEidToCustodyIdExpiryList.find(finalDestEid);
    if (itEidToCidExpiryListMap != m_mapDestEidToCustodyIdExpiryList.end()) {
        return itEidToCidExpiryListMap->second.size();
    }
    return 0;
}
