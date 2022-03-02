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

