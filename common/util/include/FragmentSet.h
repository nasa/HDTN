/**
 * @file FragmentSet.h
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
 * This FragmentSet static class provides methods to manage contiguous data fragments using
 * an std::set<data_fragment_t> in order to determine what pieces of data are missing and need retransmitted.
 * This class can be used for bytes of a packet or packet ids of a sequence of packets.
 * Contiguous data that does not abut must be split up
 * into pairs of start and end indices called a data_fragment_t.
 * This class is used by LtpFragmentSet and AGGREGATE CUSTODY SIGNAL(ACS) / CUSTODY TRANSFER ENHANCEMENT BLOCK(CTEB)
 */

#ifndef FRAGMENT_SET_H
#define FRAGMENT_SET_H 1

#define FRAGMENT_SET_USE_FREE_LIST_ALLOCATOR 1


#include <cstdint>
#include <vector>
#ifdef FRAGMENT_SET_USE_FREE_LIST_ALLOCATOR
 //#include <boost/container/set.hpp>
 //#include <boost/container/map.hpp>
 //#include <boost/container/adaptive_pool.hpp>
 #include "FreeListAllocator.h"
#endif
#include <set>
#include <map>
#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT FragmentSet {
public:
    /// Data fragment, does NOT allow overlap AND does NOT allow abut fragments
    struct HDTN_UTIL_EXPORT data_fragment_t {
        /// Begin index
        uint64_t beginIndex;
        /// End index
        uint64_t endIndex;

        /// Set begin and end indices to 0
        data_fragment_t(); //a default constructor: X()
        /**
         * Set begin and end indices.
         * @param paramBeginIndex The begin index to set.
         * @param paramEndIndex The end index to set.
         */
        data_fragment_t(uint64_t paramBeginIndex, uint64_t paramEndIndex);
        /// Default destructor
        ~data_fragment_t(); //a destructor: ~X()
        /// Default copy constructor
        data_fragment_t(const data_fragment_t& o); //a copy constructor: X(const X&)
        /// Default move constructor
        data_fragment_t(data_fragment_t&& o); //a move constructor: X(X&&)
        /// Default copy assignment operator
        data_fragment_t& operator=(const data_fragment_t& o); //a copy assignment: operator=(const X&)
        /// Default move assignment operator
        data_fragment_t& operator=(data_fragment_t&& o); //a move assignment: operator=(X&&)
        /**
         * @param o The fragment to compare.
         * @return ((beginIndex == o.beginIndex) && (endIndex == o.endIndex)).
         */
        bool operator==(const data_fragment_t & o) const; //operator ==
        /**
         * @param o The fragment to compare.
         * @return ((beginIndex != o.beginIndex) || (endIndex != o.endIndex)).
         */
        bool operator!=(const data_fragment_t & o) const; //operator !=
        /**
         * Does NOT allow overlap AND does NOT allow abut fragments.
         * @param o The fragment to compare.
         * @return ((endIndex + 1) < o.beginIndex).
         */
        bool operator<(const data_fragment_t & o) const; //operator < (no overlap no abut)
        /** Simulate searching for key in set.
         * @param key The key to search for.
         * @param keyInSet The key in the set.
         * @return True if the key could be find. or False otherwise.
         */
        static bool SimulateSetKeyFind(const data_fragment_t & key, const data_fragment_t & keyInSet);
        /** Try get the intersection of two potentially overlapping fragments.
         * @param key1 The first fragment.
         * @param key2 The second fragment.
         * @param overlap The fragment to write the intersection if found.
         * @return True if the two fragments are overlapping, or False otherwise.
         * @post if returns True, the argument to overlap is overwritten with the intersection fragment.
         */
        static bool GetOverlap(const data_fragment_t& key1, const data_fragment_t& key2, data_fragment_t& overlap);
        /** Try get the intersection of two potentially overlapping fragments.
         * Calls data_fragment_t::GetOverlap(*this, o, overlap).
         * @param o The second fragment.
         * @param overlap The fragment to write the intersection if found.
         * @return True if the two fragments are overlapping, or False otherwise..
         */
        bool GetOverlap(const data_fragment_t& o, data_fragment_t& overlap) const;
    };
    
    /// Data fragment, does NOT allow overlap AND does allow abut fragments
    struct HDTN_UTIL_EXPORT data_fragment_no_overlap_allow_abut_t : public data_fragment_t { //class which allows searching ignoring whether or not the keys abut
        /**
         * Set begin and end indices.
         * @param paramBeginIndex The begin index to set.
         * @param paramEndIndex The end index to set.
         */
        data_fragment_no_overlap_allow_abut_t(uint64_t paramBeginIndex, uint64_t paramEndIndex);
        /**
         * @param o The fragment to compare.
         * @return (endIndex < o.beginIndex).
         */
        bool operator<(const data_fragment_no_overlap_allow_abut_t& o) const;
    };

    /// Data fragment, does allow overlap AND does allow abut fragments, EXCEPT for identical pairs
    struct HDTN_UTIL_EXPORT data_fragment_unique_overlapping_t : public data_fragment_t { //class which allows everything except identical pairs
        /**
         * Set begin and end indices.
         * @param paramBeginIndex The begin index to set.
         * @param paramEndIndex The end index to set.
         */
        data_fragment_unique_overlapping_t(uint64_t paramBeginIndex, uint64_t paramEndIndex);
        /**
         * @param o The fragment to compare.
         * @return If identical begin index (endIndex < o.endIndex), else (beginIndex < o.beginIndex).
         */
        bool operator<(const data_fragment_unique_overlapping_t& o) const;
    };

#ifdef FRAGMENT_SET_USE_FREE_LIST_ALLOCATOR
    //the following will crash
    //typedef boost::container::set<data_fragment_t,
    //    std::less<data_fragment_t>,
    //    boost::container::adaptive_pool<data_fragment_t> > data_fragment_set_t;
    //typedef boost::container::set<data_fragment_no_overlap_allow_abut_t,
    //    std::less<data_fragment_no_overlap_allow_abut_t>,
    //    boost::container::adaptive_pool<data_fragment_no_overlap_allow_abut_t> > data_fragment_no_overlap_allow_abut_set_t;
    //typedef boost::container::map<data_fragment_unique_overlapping_t, uint64_t,
    //    std::less<data_fragment_unique_overlapping_t>,
    //    boost::container::adaptive_pool<std::pair<const data_fragment_unique_overlapping_t, uint64_t> > > ds_pending_map_t;
    typedef std::set < data_fragment_t,
        std::less<data_fragment_t>,
        FreeListAllocator<data_fragment_t> > data_fragment_set_t;
    typedef std::set<data_fragment_no_overlap_allow_abut_t,
        std::less<data_fragment_no_overlap_allow_abut_t>,
        FreeListAllocator<data_fragment_no_overlap_allow_abut_t> > data_fragment_no_overlap_allow_abut_set_t;
    typedef std::map<data_fragment_unique_overlapping_t, uint64_t,
        std::less<data_fragment_unique_overlapping_t>,
        FreeListAllocator<std::pair<const data_fragment_unique_overlapping_t, uint64_t> > > ds_pending_map_t;
#else
    typedef std::set<data_fragment_t> data_fragment_set_t;
    typedef std::set<data_fragment_no_overlap_allow_abut_t> data_fragment_no_overlap_allow_abut_set_t;
    typedef std::map<data_fragment_unique_overlapping_t, uint64_t> ds_pending_map_t;
#endif

public:
    /** Insert fragment in fragment set.
     *
     * If the fragment to be inserted fits entirely within an existing fragment in the fragment set, returns immediately and the fragment set is left unmodified.
     * Else, the fragment is inserted in the fragment set and all adjacent fragments (greedy) to the point of insertion are modified according to the following steps:
     * 1. If the inserted fragment overlaps an existing fragment, the union fragment created from the two takes the place of the existing fragment in the fragment set.
     * 2. All abut fragments to the inserted fragment, including the inserted fragment, are reduced (condensed) to a single union fragment as well.
     * 3. The rest of the fragments in the fragment set, that at this point are NEITHER overlapping-with NOR abut-to the inserted fragment, remain as-is.
     * @param fragmentSet The fragment set to modify.
     * @param key The fragment to insert.
     * @return True if the fragment was inserted successfully (and thus the fragment set was modified), or False otherwise.
     * @post If returns True, the argument to fragmentSet is modified accordingly (see above for details).
     */
    static bool InsertFragment(data_fragment_set_t & fragmentSet, data_fragment_t key);
    
    /** Get all fragments NOT present within the given bounds.
     *
     * Calculates the set difference for fragmentSet.
     * @param bounds The bounds to remain within.
     * @param fragmentSet The fragment set.
     * @param boundsMinusFragmentsSet The difference fragment set to modify.
     * @post The argument to boundsMinusFragmentsSet is overwritten to hold the resulting difference fragment set.
     */
    static void GetBoundsMinusFragments(const data_fragment_t bounds, const data_fragment_set_t& fragmentSet, data_fragment_set_t& boundsMinusFragmentsSet);

    /** Query whether fragment fits entirely within an existing fragment in the fragment set.
     *
     * @param fragmentSet The fragment set.
     * @param key The fragment to query.
     * @return True if the fragment fits entirely within an existing fragment in the fragment set, or False otherwise.
     */
    static bool ContainsFragmentEntirely(const data_fragment_set_t& fragmentSet, const data_fragment_t& key);
    
    /** Query whether fragment overlaps with an existing fragment in the fragment set.
     *
     * This function is NOT functionally equivalent to the inverse of FragmentSet::ContainsFragmentEntirely(), this functions checks for any overlap while the
     * latter function checks only for the strict case where both left and right bounds of the overlap are entirely contained within the existing overlapping fragment.
     * @param fragmentSet The fragment set.
     * @param key The fragment to query.
     * @return True if the fragment DOES NOT overlap with an existing fragment in the fragment set,
     *         False if the fragment overlaps with an existing fragment in the fragment set.
     */
    static bool DoesNotContainFragmentEntirely(const data_fragment_set_t& fragmentSet, const data_fragment_t& key);

    /** Query whether there is any overlap between the two fragment sets.
     *
     * @param fragmentSet1 The first fragment set.
     * @param fragmentSet2 The second fragment set.
     * @return True if the fragment sets overlap each other, or False otherwise.
     */
    static bool FragmentSetsHaveOverlap(const data_fragment_set_t& fragmentSet1, const data_fragment_set_t& fragmentSet2);

    /** Remove fragment from fragment set.
     *
     * If the fragment does not exist in the fragment set, returns immediately and the fragment set is left unmodified.
     * Else, the fragment is removed from the fragment set and all directly affected fragments from the range removed are modified according to the following steps:
     * 1. If the fragment to remove fits entirely within an existing fragment, the range is removed and the existing fragment is thus split into either one or two fragments.
     * 2. All existing fragments that fit entirely within the fragment to remove, are simply removed.
     * 3. Any existing fragments that overlap with the fragment to remove, are trimmed accordingly.
     * 4. The rest of the fragments in the fragment set, that at this point are NOT overlapping with the fragment to remove, remain as-is.
     * @param fragmentSet The fragment set to modify.
     * @param key The fragment to remove.
     * @return True if the fragment was removed (and thus the fragment set was modified), or False otherwise.
     * @post If returns True, the argument to fragmentSet is modified accordingly (see above for details).
     */
    static bool RemoveFragment(data_fragment_set_t& fragmentSet, const data_fragment_t& key);

    /** Print fragment set.
     *
     * Convenience function to log a fragment set.
     * @param fragmentSet The fragment set.
     */
    static void PrintFragmentSet(const data_fragment_set_t& fragmentSet);
};

#endif // FRAGMENT_SET_H

