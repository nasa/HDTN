/**
 * @file LtpFragmentSet.cpp
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
 */

#include "LtpFragmentSet.h"
#include <iostream>
#include "Sdnv.h"

bool LtpFragmentSet::PopulateReportSegment(const std::set<data_fragment_t> & fragmentSet, Ltp::report_segment_t & reportSegment, uint64_t lowerBound, uint64_t upperBound) {
    if (fragmentSet.empty()) {
        return false;
    }

    //Lower bound : The lower bound of a report segment is the size of the(interior) block prefix to which the segment's reception claims do NOT pertain.
    std::set<data_fragment_t>::const_iterator firstElement;
    if (lowerBound == UINT64_MAX) { //AUTO DETECT
        firstElement = fragmentSet.cbegin();
        lowerBound = firstElement->beginIndex;
    }
    else {
        //set::lower_bound() returns an iterator pointing to the element in the container which is equivalent to k passed in the parameter.
        //In case k is not present in the set container, the function returns an iterator pointing to the immediate next element which is just greater than k.
        firstElement = fragmentSet.lower_bound(data_fragment_t(lowerBound, lowerBound)); //firstElement may overlap or abut key
    }
    reportSegment.lowerBound = lowerBound;

    //Upper bound : The upper bound of a report segment is the size of the block prefix to which the segment's reception claims pertain.
    if (upperBound == UINT64_MAX) { //AUTO DETECT
        std::set<data_fragment_t>::const_reverse_iterator lastElement = fragmentSet.crbegin();
        upperBound = lastElement->endIndex + 1;
    }
    reportSegment.upperBound = upperBound;
    if (lowerBound >= upperBound) {
        return false;
    }
    const uint64_t differenceBetweenUpperAndLowerBounds = upperBound - lowerBound;

    //Reception claims
    reportSegment.receptionClaims.clear();
    reportSegment.receptionClaims.reserve(fragmentSet.size());
    for (std::set<data_fragment_t>::const_iterator it = firstElement; it != fragmentSet.cend(); ++it) {
        //Offset : The offset indicates the successful reception of data beginning at the indicated offset from the lower bound of the RS.The
        //offset within the entire block can be calculated by summing this offset with the lower bound of the RS.
        const uint64_t beginIndex = std::max(it->beginIndex, lowerBound);
        if (beginIndex >= upperBound) {
            break;
        }
        const uint64_t offset = beginIndex - lowerBound;

        //Length : The length of a reception claim indicates the number of contiguous octets of block data starting at the indicated offset that have been successfully received.
        uint64_t length = (it->endIndex + 1) - beginIndex;

        //A reception claim's length shall never exceed the difference between the upper and lower bounds of the report segment.
        length = std::min(length, differenceBetweenUpperAndLowerBounds);

        //The sum of a reception claim's offset and length and the lower bound of the report segment shall never exceed the upper bound of the report segment.
        // offset + length + lowerBound <= upperBound
        // length <= (upperBound - lowerBound) - offset
        // length <= (upperBound - lowerBound) - (beginIndex - lowerBound)
        // length <= upperBound - lowerBound - beginIndex + lowerBound
        // length <= upperBound - beginIndex
        length = std::min(length, upperBound - beginIndex);
        
        if (length) { //A reception claim's length shall never be less than 1 
            reportSegment.receptionClaims.emplace_back(offset, length);
        }
    }
    return true;
}

bool LtpFragmentSet::SplitReportSegment(const Ltp::report_segment_t & originalTooLargeReportSegment, std::vector<Ltp::report_segment_t> & reportSegmentsVec, const uint64_t maxReceptionClaimsPerReportSegment) {
    //3.2.  Retransmission
    //
    //... The maximum size of a report segment, like
    //all LTP segments, is constrained by the data - link MTU; if many non -
    //contiguous segments were lost in a large block transmission and/or
    //the data - link MTU was relatively small, multiple report segments need
    //to be generated.  In this case, LTP generates as many report segments
    //as are necessary and splits the scope of red - part data covered across
    //multiple report segments so that each of them may stand on their own.
    //For example, if three report segments are to be generated as part of
    //a reception report covering red - part data in range[0:1,000,000],
    //they could look like this: RS 19, scope[0:300,000], RS 20, scope
    //[300,000:950,000], and RS 21, scope[950,000:1,000,000].  In all
    //cases, a timer is started upon transmission of each report segment of
    //the reception report.
    if (maxReceptionClaimsPerReportSegment == 0) {
        return false;
    }
    reportSegmentsVec.resize(0);
    reportSegmentsVec.reserve((originalTooLargeReportSegment.receptionClaims.size() / maxReceptionClaimsPerReportSegment) + 1);
    const uint64_t originalLowerBound = originalTooLargeReportSegment.lowerBound;
    uint64_t thisRsNewLowerBound = originalTooLargeReportSegment.lowerBound;
    std::vector<Ltp::reception_claim_t>::const_iterator it = originalTooLargeReportSegment.receptionClaims.cbegin();
    while (it != originalTooLargeReportSegment.receptionClaims.cend()) {
        reportSegmentsVec.emplace_back();
        Ltp::report_segment_t & rs = reportSegmentsVec.back();
        rs.receptionClaims.reserve(maxReceptionClaimsPerReportSegment);
        rs.lowerBound = thisRsNewLowerBound;
        const uint64_t deltaLowerBound = thisRsNewLowerBound - originalLowerBound;
        for (uint64_t i = 0; (i < maxReceptionClaimsPerReportSegment) && (it != originalTooLargeReportSegment.receptionClaims.cend()); ++it, ++i) {
            rs.receptionClaims.emplace_back(it->offset - deltaLowerBound, it->length);
            thisRsNewLowerBound = originalLowerBound + it->offset + it->length;
            rs.upperBound = thisRsNewLowerBound; //next lower bound will be the last upper bound
        }
    }
    reportSegmentsVec.back().upperBound = originalTooLargeReportSegment.upperBound;
    return true;
}

void LtpFragmentSet::AddReportSegmentToFragmentSet(std::set<data_fragment_t> & fragmentSet, const Ltp::report_segment_t & reportSegment) {
    const uint64_t lowerBound = reportSegment.lowerBound;
    for (std::vector<Ltp::reception_claim_t>::const_iterator it = reportSegment.receptionClaims.cbegin(); it != reportSegment.receptionClaims.cend(); ++it) {
        const uint64_t beginIndex = lowerBound + it->offset;
        InsertFragment(fragmentSet, data_fragment_t(beginIndex, (beginIndex + it->length) - 1));
    }
}

void LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent(std::set<data_fragment_t> & fragmentSetNeedingResent, const Ltp::report_segment_t & reportSegment) {
    const std::vector<Ltp::reception_claim_t> & receptionClaims = reportSegment.receptionClaims;
    if (receptionClaims.empty()) {
        return;
    }
    const uint64_t lowerBound = reportSegment.lowerBound;
    std::vector<Ltp::reception_claim_t>::const_iterator it = receptionClaims.cbegin();
    if (it->offset > 0) { //add one
        InsertFragment(fragmentSetNeedingResent, data_fragment_t(lowerBound, (lowerBound + it->offset) - 1));
    }
    //uint64_t nextBeginIndex = lowerBound;
    const Ltp::reception_claim_t * previousReceptionClaim = NULL;
    for (std::vector<Ltp::reception_claim_t>::const_iterator it = reportSegment.receptionClaims.cbegin(); it != reportSegment.receptionClaims.cend(); ++it) {
        if (previousReceptionClaim) {
            const uint64_t beginIndex = lowerBound + previousReceptionClaim->offset + previousReceptionClaim->length;
            const uint64_t endIndex = (lowerBound + it->offset) - 1;
            InsertFragment(fragmentSetNeedingResent, data_fragment_t(beginIndex, endIndex));
        }
        //nextBeginIndex = (lowerBound + it->offset + it->length);
        previousReceptionClaim = &(*it);
    }
    const uint64_t beginIndex = lowerBound + previousReceptionClaim->offset + previousReceptionClaim->length;;
    if (beginIndex < reportSegment.upperBound) {
        InsertFragment(fragmentSetNeedingResent, data_fragment_t(beginIndex, reportSegment.upperBound - 1));
    }
}
