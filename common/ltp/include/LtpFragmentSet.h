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
#include "Ltp.h"
#include "FragmentSet.h"

class LTP_LIB_EXPORT LtpFragmentSet : public FragmentSet {

public:
    static bool PopulateReportSegment(const std::set<data_fragment_t> & fragmentSet, Ltp::report_segment_t & reportSegment, uint64_t lowerBound = UINT64_MAX, uint64_t upperBound = UINT64_MAX);
    static bool SplitReportSegment(const Ltp::report_segment_t & originalTooLargeReportSegment, std::vector<Ltp::report_segment_t> & reportSegmentsVec, const uint64_t maxReceptionClaimsPerReportSegment);
    static void AddReportSegmentToFragmentSet(std::set<data_fragment_t> & fragmentSet, const Ltp::report_segment_t & reportSegment);
    static void AddReportSegmentToFragmentSetNeedingResent(std::set<data_fragment_t> & fragmentSetNeedingResent, const Ltp::report_segment_t & reportSegment);
};

#endif // LTP_FRAGMENT_SET_H

