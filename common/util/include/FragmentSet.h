/**
 * @file FragmentSet.h
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
 * This FragmentSet static class provides methods to manage contiguous data fragments using
 * an std::set<data_fragment_t> in order to determine what pieces of data are missing and need retransmitted.
 * This class can be used for bytes of a packet or packet ids of a sequence of packets.
 * Contiguous data that does not abut must be split up
 * into pairs of start and end indices called a data_fragment_t.
 * This class is used by LtpFragmentSet and AGGREGATE CUSTODY SIGNAL(ACS) / CUSTODY TRANSFER ENHANCEMENT BLOCK(CTEB)
 */

#ifndef FRAGMENT_SET_H
#define FRAGMENT_SET_H 1

#include <cstdint>
#include <vector>
#include <set>
#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT FragmentSet {
public:
    struct HDTN_UTIL_EXPORT data_fragment_t {
        uint64_t beginIndex;
        uint64_t endIndex;

        data_fragment_t(); //a default constructor: X()
        data_fragment_t(uint64_t paramBeginIndex, uint64_t paramEndIndex);
        ~data_fragment_t(); //a destructor: ~X()
        data_fragment_t(const data_fragment_t& o); //a copy constructor: X(const X&)
        data_fragment_t(data_fragment_t&& o); //a move constructor: X(X&&)
        data_fragment_t& operator=(const data_fragment_t& o); //a copy assignment: operator=(const X&)
        data_fragment_t& operator=(data_fragment_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const data_fragment_t & o) const; //operator ==
        bool operator!=(const data_fragment_t & o) const; //operator !=
        bool operator<(const data_fragment_t & o) const; //operator < (no overlap no abut)
        static bool SimulateSetKeyFind(const data_fragment_t & key, const data_fragment_t & keyInSet);
    };

    struct HDTN_UTIL_EXPORT data_fragment_no_overlap_allow_abut_t : public data_fragment_t { //class which allows searching ignoring whether or not the keys abut
        data_fragment_no_overlap_allow_abut_t(uint64_t paramBeginIndex, uint64_t paramEndIndex);
        bool operator<(const data_fragment_no_overlap_allow_abut_t& o) const;
    };

    struct HDTN_UTIL_EXPORT data_fragment_unique_overlapping_t : public data_fragment_t { //class which allows everything except identical pairs
        data_fragment_unique_overlapping_t(uint64_t paramBeginIndex, uint64_t paramEndIndex);
        bool operator<(const data_fragment_unique_overlapping_t& o) const;
    };

public:
    //return true if the set was modified, false if unmodified
    static bool InsertFragment(std::set<data_fragment_t> & fragmentSet, data_fragment_t key);

    
    static void GetBoundsMinusFragments(const data_fragment_t bounds, const std::set<data_fragment_t>& fragmentSet, std::set<data_fragment_t>& boundsMinusFragmentsSet);

    static bool ContainsFragmentEntirely(const std::set<data_fragment_t> & fragmentSet, const data_fragment_t & key);
    static bool DoesNotContainFragmentEntirely(const std::set<data_fragment_t> & fragmentSet, const data_fragment_t & key);

    //return true if the set was modified, false if unmodified
    static bool RemoveFragment(std::set<data_fragment_t> & fragmentSet, const data_fragment_t & key);

    static void PrintFragmentSet(const std::set<data_fragment_t> & fragmentSet);
};

#endif // FRAGMENT_SET_H

