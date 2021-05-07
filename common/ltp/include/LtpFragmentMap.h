#ifndef LTP_FRAGMENT_MAP_H
#define LTP_FRAGMENT_MAP_H 1

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/integer.hpp>
#include <boost/function.hpp>
#include <vector>
#include <map>
#include <set>
#include "Ltp.h"

class LtpFragmentMap {
public:
    struct data_fragment_t {
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

public:
    
    static void InsertFragment(std::set<data_fragment_t> & fragmentSet, data_fragment_t key);
    static bool PopulateReportSegment(const std::set<data_fragment_t> & fragmentSet, Ltp::report_segment_t & reportSegment);
    static void AddReportSegmentToFragmentSet(std::set<data_fragment_t> & fragmentSet, const Ltp::report_segment_t & reportSegment);
    static void AddReportSegmentToFragmentSetNeedingResent(std::set<data_fragment_t> & fragmentSetNeedingResent, const Ltp::report_segment_t & reportSegment);
    static void PrintFragmentSet(const std::set<data_fragment_t> & fragmentSet);

    LtpFragmentMap(); //a default constructor: X()
    ~LtpFragmentMap(); //a destructor: ~X()
    /*LtpFragmentMap(const LtpFragmentMap& o); //a copy constructor: X(const X&)
    LtpFragmentMap(LtpFragmentMap&& o); //a move constructor: X(X&&)
    LtpFragmentMap& operator=(const LtpFragmentMap& o); //a copy assignment: operator=(const X&)
    LtpFragmentMap& operator=(LtpFragmentMap&& o); //a move assignment: operator=(X&&)
    */
    //bool AddDataRange(uint64_t startIndex, uint64_t length);

private:
    //std::map<uint64_t, uint64_t> m_fragmentMap; //startIndex, length
    
};

#endif // LTP_FRAGMENT_MAP_H

