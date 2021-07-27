#include "FragmentSet.h"
#include <iostream>
#include "Sdnv.h"
#include <algorithm>


FragmentSet::FragmentSet() { } //a default constructor: X()
FragmentSet::~FragmentSet() { } //a destructor: ~X()
FragmentSet::data_fragment_t::data_fragment_t(uint64_t paramBeginIndex, uint64_t paramEndIndex) : beginIndex(paramBeginIndex), endIndex(paramEndIndex) { }
FragmentSet::data_fragment_t::data_fragment_t() : beginIndex(0), endIndex(0) { } //a default constructor: X()
FragmentSet::data_fragment_t::~data_fragment_t() { } //a destructor: ~X()
FragmentSet::data_fragment_t::data_fragment_t(const data_fragment_t& o) : beginIndex(o.beginIndex), endIndex(o.endIndex) { } //a copy constructor: X(const X&)
FragmentSet::data_fragment_t::data_fragment_t(data_fragment_t&& o) : beginIndex(o.beginIndex), endIndex(o.endIndex) { } //a move constructor: X(X&&)
FragmentSet::data_fragment_t& FragmentSet::data_fragment_t::operator=(const data_fragment_t& o) { //a copy assignment: operator=(const X&)
    beginIndex = o.beginIndex;
    endIndex = o.endIndex;
    return *this;
}
FragmentSet::data_fragment_t& FragmentSet::data_fragment_t::operator=(data_fragment_t && o) { //a move assignment: operator=(X&&)
    beginIndex = o.beginIndex;
    endIndex = o.endIndex;
    return *this;
}
bool FragmentSet::data_fragment_t::operator==(const data_fragment_t & o) const {
    return (beginIndex == o.beginIndex) && (endIndex == o.endIndex);
}
bool FragmentSet::data_fragment_t::operator!=(const data_fragment_t & o) const {
    return (beginIndex != o.beginIndex) || (endIndex != o.endIndex);
}
bool FragmentSet::data_fragment_t::operator<(const data_fragment_t & o) const { //operator < (no overlap no abut) 
    return ((endIndex+1) < o.beginIndex);
}
bool FragmentSet::data_fragment_t::SimulateSetKeyFind(const data_fragment_t & key, const data_fragment_t & keyInSet) {
    //equal defined as !(a<b) && !(b<a)
    return !(key < keyInSet) && !(keyInSet < key);
}


void FragmentSet::InsertFragment(std::set<data_fragment_t> & fragmentSet, data_fragment_t key) {
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

bool FragmentSet::ContainsFragmentEntirely(const std::set<data_fragment_t> & fragmentSet, data_fragment_t key) {
    std::set<data_fragment_t>::const_iterator res = fragmentSet.find(key);
    if (res == fragmentSet.cend()) { //not found (nothing that overlaps or abuts)
        return false;
    }
    const uint64_t keyInMapBeginIndex = res->beginIndex;
    const uint64_t keyInMapEndIndex = res->endIndex;
    return ((key.beginIndex >= keyInMapBeginIndex) && (key.endIndex <= keyInMapEndIndex)); //new key fits entirely inside existing key (set needs no modification)
}


void FragmentSet::PrintFragmentSet(const std::set<data_fragment_t> & fragmentSet) {
    for (std::set<data_fragment_t>::const_iterator it = fragmentSet.cbegin(); it != fragmentSet.cend(); ++it) {
        std::cout << "(" << it->beginIndex << "," << it->endIndex << ") ";
    }
    std::cout << std::endl;
}
