/**
 * @file CustodyTimers.h
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
 *
 * @section DESCRIPTION
 *
 * This CustodyTimers class defines methods for knowing when to retransmit a bundle from storage.
 */

#ifndef _CUSTODY_TIMERS_H
#define _CUSTODY_TIMERS_H 1

#include <cstdint>
#include <map>
#include <list>
#include <utility>
#include <string>
#include "codec/bpv6.h"
#include <boost/date_time.hpp>
#include "storage_lib_export.h"

class CustodyTimers {
private:
    CustodyTimers();
public:
    
    STORAGE_LIB_EXPORT CustodyTimers(const boost::posix_time::time_duration & timeout);
    STORAGE_LIB_EXPORT ~CustodyTimers();

    STORAGE_LIB_EXPORT bool PollOneAndPopExpiredCustodyTimer(uint64_t & custodyId, const std::vector<cbhe_eid_t> & availableDestEids, const boost::posix_time::ptime & nowPtime);
    STORAGE_LIB_EXPORT bool PollOneAndPopAnyExpiredCustodyTimer(uint64_t & custodyId, const boost::posix_time::ptime & nowPtime);
    STORAGE_LIB_EXPORT bool StartCustodyTransferTimer(const cbhe_eid_t & finalDestEid, const uint64_t custodyId);
    STORAGE_LIB_EXPORT bool CancelCustodyTransferTimer(const cbhe_eid_t & finalDestEid, const uint64_t custodyId);
    STORAGE_LIB_EXPORT std::size_t GetNumCustodyTransferTimers();
    STORAGE_LIB_EXPORT std::size_t GetNumCustodyTransferTimers(const cbhe_eid_t & finalDestEid);

protected:
    typedef std::pair<uint64_t, boost::posix_time::ptime> custid_ptime_pair_t;
    typedef std::list<custid_ptime_pair_t> custid_ptime_list_t;
    typedef std::map<uint64_t, custid_ptime_list_t::iterator> custid_to_listiterator_map_t;
    typedef std::pair<uint64_t, custid_ptime_list_t::iterator> custid_to_listiterator_map_insertion_element_t;
    typedef std::map<cbhe_eid_t, custid_ptime_list_t> desteid_to_custidexpirylist_map_t;

    desteid_to_custidexpirylist_map_t m_mapDestEidToCustodyIdExpiryList;
    custid_to_listiterator_map_t m_mapCustodyIdToListIterator;

    const boost::posix_time::time_duration M_CUSTODY_TIMEOUT_DURATION;
};


#endif //_CUSTODY_TIMERS_H
