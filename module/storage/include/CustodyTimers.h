#ifndef _CUSTODY_TIMERS_H
#define _CUSTODY_TIMERS_H

#include <cstdint>
#include <map>
#include <list>
#include <utility>
#include <string>
#include "codec/bpv6.h"
#include <boost/date_time.hpp>


class CustodyTimers {
private:
    CustodyTimers();
public:
    
    CustodyTimers(const boost::posix_time::time_duration & timeout);
    ~CustodyTimers();

    bool PollOneAndPopExpiredCustodyTimer(uint64_t & custodyId, const std::vector<cbhe_eid_t> & availableDestEids, const boost::posix_time::ptime & nowPtime);
    bool StartCustodyTransferTimer(const cbhe_eid_t & finalDestEid, const uint64_t custodyId);
    bool CancelCustodyTransferTimer(const cbhe_eid_t & finalDestEid, const uint64_t custodyId);
    std::size_t GetNumCustodyTransferTimers();
    std::size_t GetNumCustodyTransferTimers(const cbhe_eid_t & finalDestEid);

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
