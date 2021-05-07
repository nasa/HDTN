#include "LtpFragmentMap.h"
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "Sdnv.h"



LtpFragmentMap::LtpFragmentMap() { } //a default constructor: X()
LtpFragmentMap::~LtpFragmentMap() { } //a destructor: ~X()
LtpFragmentMap::data_fragment_t::data_fragment_t(uint64_t paramBeginIndex, uint64_t paramEndIndex) : beginIndex(paramBeginIndex), endIndex(paramEndIndex) { }
LtpFragmentMap::data_fragment_t::data_fragment_t() : beginIndex(0), endIndex(0) { } //a default constructor: X()
LtpFragmentMap::data_fragment_t::~data_fragment_t() { } //a destructor: ~X()
LtpFragmentMap::data_fragment_t::data_fragment_t(const data_fragment_t& o) : beginIndex(o.beginIndex), endIndex(o.endIndex) { } //a copy constructor: X(const X&)
LtpFragmentMap::data_fragment_t::data_fragment_t(data_fragment_t&& o) : beginIndex(o.beginIndex), endIndex(o.endIndex) { } //a move constructor: X(X&&)
LtpFragmentMap::data_fragment_t& LtpFragmentMap::data_fragment_t::operator=(const data_fragment_t& o) { //a copy assignment: operator=(const X&)
    beginIndex = o.beginIndex;
    endIndex = o.endIndex;
    return *this;
}
LtpFragmentMap::data_fragment_t& LtpFragmentMap::data_fragment_t::operator=(data_fragment_t && o) { //a move assignment: operator=(X&&)
    beginIndex = o.beginIndex;
    endIndex = o.endIndex;
    return *this;
}
bool LtpFragmentMap::data_fragment_t::operator==(const data_fragment_t & o) const {
    return (beginIndex == o.beginIndex) && (endIndex == o.endIndex);
}
bool LtpFragmentMap::data_fragment_t::operator!=(const data_fragment_t & o) const {
    return (beginIndex != o.beginIndex) || (endIndex != o.endIndex);
}
bool LtpFragmentMap::data_fragment_t::operator<(const data_fragment_t & o) const { //operator < (no overlap no abut) 
    return ((endIndex+1) < o.beginIndex);
}
bool LtpFragmentMap::data_fragment_t::SimulateSetKeyFind(const data_fragment_t & key, const data_fragment_t & keyInSet) {
    //equal defined as !(a<b) && !(b<a)
    return !(key < keyInSet) && !(keyInSet < key);
}


void LtpFragmentMap::InsertFragment(std::set<data_fragment_t> & fragmentSet, data_fragment_t key) {
    while (true) {
        std::pair<std::set<data_fragment_t>::iterator, bool> res = fragmentSet.insert(key);
        if (res.second == true) { //fragment key was inserted with no overlap nor abut
            return;
        }
        //fragment key not inserted due to overlap or abut.. res.first points to element in the set that may need expanded to fit new key fragment
        const uint64_t keyInMapBeginIndex = res.first->beginIndex;
        const uint64_t keyInMapEndIndex = res.first->endIndex;
        if ((key.beginIndex >= keyInMapBeginIndex) && (key.endIndex <= keyInMapEndIndex)) { //new key fits entirely inside existing key (set needs no modification)
            return;
        }
        key.beginIndex = std::min(key.beginIndex, keyInMapBeginIndex);
        key.endIndex = std::max(key.endIndex, keyInMapEndIndex);
        fragmentSet.erase(res.first);
    }
}

bool LtpFragmentMap::PopulateReportSegment(const std::set<data_fragment_t> & fragmentSet, Ltp::report_segment_t & reportSegment, uint64_t lowerBound, uint64_t upperBound) {
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

void LtpFragmentMap::AddReportSegmentToFragmentSet(std::set<data_fragment_t> & fragmentSet, const Ltp::report_segment_t & reportSegment) {
    const uint64_t lowerBound = reportSegment.lowerBound;
    for (std::vector<Ltp::reception_claim_t>::const_iterator it = reportSegment.receptionClaims.cbegin(); it != reportSegment.receptionClaims.cend(); ++it) {
        const uint64_t beginIndex = lowerBound + it->offset;
        LtpFragmentMap::InsertFragment(fragmentSet, LtpFragmentMap::data_fragment_t(beginIndex, (beginIndex + it->length) - 1));
    }
}

void LtpFragmentMap::AddReportSegmentToFragmentSetNeedingResent(std::set<data_fragment_t> & fragmentSetNeedingResent, const Ltp::report_segment_t & reportSegment) {
    const std::vector<Ltp::reception_claim_t> & receptionClaims = reportSegment.receptionClaims;
    if (receptionClaims.empty()) {
        return;
    }
    const uint64_t lowerBound = reportSegment.lowerBound;
    std::vector<Ltp::reception_claim_t>::const_iterator it = receptionClaims.cbegin();
    if (it->offset > lowerBound) { //add one
        LtpFragmentMap::InsertFragment(fragmentSetNeedingResent, LtpFragmentMap::data_fragment_t(lowerBound, (lowerBound + it->offset) - 1));
    }
    //uint64_t nextBeginIndex = lowerBound;
    const Ltp::reception_claim_t * previousReceptionClaim = NULL;
    for (std::vector<Ltp::reception_claim_t>::const_iterator it = reportSegment.receptionClaims.cbegin(); it != reportSegment.receptionClaims.cend(); ++it) {
        if (previousReceptionClaim) {
            const uint64_t beginIndex = lowerBound + previousReceptionClaim->offset + previousReceptionClaim->length;
            const uint64_t endIndex = (lowerBound + it->offset) - 1;
            LtpFragmentMap::InsertFragment(fragmentSetNeedingResent, LtpFragmentMap::data_fragment_t(beginIndex, endIndex));
        }
        //nextBeginIndex = (lowerBound + it->offset + it->length);
        previousReceptionClaim = &(*it);
    }
    const uint64_t beginIndex = lowerBound + previousReceptionClaim->offset + previousReceptionClaim->length;;
    if (beginIndex < reportSegment.upperBound) {
        LtpFragmentMap::InsertFragment(fragmentSetNeedingResent, LtpFragmentMap::data_fragment_t(beginIndex, reportSegment.upperBound - 1));
    }
}

void LtpFragmentMap::PrintFragmentSet(const std::set<data_fragment_t> & fragmentSet) {
    for (std::set<data_fragment_t>::const_iterator it = fragmentSet.cbegin(); it != fragmentSet.cend(); ++it) {
        std::cout << "(" << it->beginIndex << "," << it->endIndex << ") ";
    }
    std::cout << std::endl;
}