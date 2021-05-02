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
//bool LtpFragmentMap::data_fragment_t::operator||(const data_fragment_t & o) const { //operator || (overlaps or abuts)
//    return (beginIndex != o.beginIndex) || (endIndex != o.endIndex);
//}
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
/*
bool LtpFragmentMap::AddDataRange(uint64_t startIndex, uint64_t length) {
    if (m_fragmentMap.empty()) {
        m_fragmentMap[startIndex] = length;
        return true;
    }
    //todo just use lower bound and --it for before
    std::pair<std::map<uint64_t, uint64_t>::iterator, std::map<uint64_t, uint64_t>::iterator> range = m_fragmentMap.equal_range(startIndex);
    uint64_t startIndexLowerBound = 0;
    uint64_t lengthLowerBound = 0;
    if (range.first != m_fragmentMap.end()) { //lower_bound (not less than key) 
        startIndexLowerBound = range.first->first;
        lengthLowerBound = range.first->second;
    }
    uint64_t startIndexUpperBound = UINT64_MAX;
    uint64_t lengthUpperBound = 0;
    if (range.second != m_fragmentMap.end()) { //upper_bound (pointing to the first element greater than key) 
        startIndexUpperBound = range.second->first;
        lengthUpperBound = range.second->second;
    }
    const uint64_t dataRangeFirstOutOfBoundsIndex = startIndex + length;
    const uint64_t firstWritableIndex = startIndexLowerBound + lengthLowerBound;
    if ((dataRangeFirstOutOfBoundsIndex <= startIndexUpperBound) && (startIndex >= firstWritableIndex)) {
        m_fragmentMap[startIndex] = length;
    }
}*/