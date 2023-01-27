/**
 * @file LtpFragmentSet.h
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
 * This LtpFragmentSet static class handles all functions required for creating
 * or receiving/processing LTP report segments.  It is a child class of util/FragmentSet.
 */

#ifndef LTP_FRAGMENT_SET_H
#define LTP_FRAGMENT_SET_H 1

#include <boost/integer.hpp>
#include <boost/function.hpp>
#include <vector>
#include <set>
#include <map>
#include <list>
#include "Ltp.h"
#include "FragmentSet.h"

class LTP_LIB_EXPORT LtpFragmentSet : public FragmentSet {

public:
    /** Parse report segment from fragment set.
     *
     * If the fragment set is malformed, returns immediately and the report segment is left unmodified.
     * Else, the bounds and reception claims of the resulting report segment are parsed from the fragment set.
     * @param fragmentSet The fragment set.
     * @param reportSegment The report segment to modify.
     * @param lowerBound The lower bound, default value covers the entire range on the left.
     * @param upperBound The upper bound, default value covers the entire range on the right.
     * @return True if the report segment could be populated successfully (and thus the report segment was modified), or False otherwise.
     * @post If returns True, the argument to reportSegment is modified accordingly (see above for details).
     */
    static bool PopulateReportSegment(const data_fragment_set_t & fragmentSet, Ltp::report_segment_t & reportSegment, uint64_t lowerBound = UINT64_MAX, uint64_t upperBound = UINT64_MAX);
    
    /** Split report segment by reception claims.
     *
     * If the split factor is set to 0, returns immediately and the report segment vector is left unmodified.
     * Else, the reception claims of the report segment to split are grouped every maxReceptionClaimsPerReportSegment claims and placed in their own
     * report segment, the last report segment in the vector may be left with a number of claims LESS-THAN maxReceptionClaimsPerReportSegment on an odd split.
     *
     * This function is typically used to split an excessively large report segment (as per MTU constraints) into multiple smaller report segments.
     * @param originalTooLargeReportSegment The report segment to split.
     * @param reportSegmentsVec The report segment vector to modify.
     * @param maxReceptionClaimsPerReportSegment The maximum number of reception claims per resulting report segment, this split factor.
     * @return True if the report segment could be split successfully (and thus the report segment vector was modified), or False otherwise.
     * @post If returns True, the argument to reportSegmentsVec is modified accordingly (see above for details).
     */
    static bool SplitReportSegment(const Ltp::report_segment_t& originalTooLargeReportSegment, std::vector<Ltp::report_segment_t>& reportSegmentsVec, const uint64_t maxReceptionClaimsPerReportSegment);
    
    /** Insert already-received fragments to fragment set.
     *
     * Functionally equivalent to calling FragmentSet::InsertFragment() (see docs for details) on each fragment covered by the report segment reception claims.
     * @param fragmentSet The fragment set to modify.
     * @param reportSegment The report segment.
     * @return True if the fragment set was modified, or False otherwise.
     * @post If returns True, the argument to fragmentSet is modified accordingly (see above for details).
     * @post Calling both LtpFragmentSet::AddReportSegmentToFragmentSet() and LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent() in any order
     * on the same report segment and fragment set, will result in the fragment set containing a single fragment spanning the entire scope of the report segment.
     */
    static bool AddReportSegmentToFragmentSet(data_fragment_set_t& fragmentSet, const Ltp::report_segment_t& reportSegment);

    /** Insert needing-retransmitted fragments to fragment set.
     *
     * Functionally equivalent to calling FragmentSet::InsertFragment() (see docs for details) on each fragment NOT covered by the report segment reception claims.
     * @param fragmentSetNeedingResent The fragment set to modify.
     * @param reportSegment The report segment.
     * @return True if the fragment set was modified, or False otherwise.
     * @post If returns True, the argument to fragmentSetNeedingResent is modified accordingly (see above for details).
     * @post Calling both LtpFragmentSet::AddReportSegmentToFragmentSet() and LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent() in any order
     * on the same report segment and fragment set, will result in the fragment set containing a single fragment spanning the entire scope of the report segment.
     */
    static bool AddReportSegmentToFragmentSetNeedingResent(data_fragment_set_t& fragmentSetNeedingResent, const Ltp::report_segment_t& reportSegment);

    /** Recalculate the currently reported state from any given number of report segments.
     *
     * Given a map of report segment bounds and the already-received fragments, recalculates the effective scope of each report segment still needing further
     * processing and for each resulting report populates a fragment set of fragments needing retransmission.
     * @param rsBoundsToRsnMap The report segment bounds, mapped by report serial number.
     * @param allReceivedFragmentsSet The already-received fragment set.
     * @param listFragmentSetNeedingResentForEachReport The list of needing-retransmitted fragment set per report segment, to modify.
     * @post The argument to listFragmentSetNeedingResentForEachReport is modified accordingly (see above for details).
     */
    static void ReduceReportSegments(const ds_pending_map_t& rsBoundsToRsnMap,
        const data_fragment_set_t& allReceivedFragmentsSet,
        std::list<std::pair<uint64_t, data_fragment_set_t > >& listFragmentSetNeedingResentForEachReport);
};

#endif // LTP_FRAGMENT_SET_H

