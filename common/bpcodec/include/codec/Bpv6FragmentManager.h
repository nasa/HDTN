#ifndef BPV6_FRAGMENT_MANAGER_H
#define BPV6_FRAGMENT_MANAGER_H

#include "BundleViewV6.h"
#include "FragmentSet.h"
#include <list>
#include <boost/thread/mutex.hpp>

class Bpv6FragmentManager {
public:
    bool AddFragmentAndGetComplete(uint8_t *data, size_t len, bool & isComplete, BundleViewV6 & assembledBv);
    bool AddFragmentAndGetComplete_ThreadSafe(uint8_t *data, size_t len, bool & isComplete, BundleViewV6 & assembledBv);

private:
    struct Bpv6Id {
        cbhe_eid_t src;
        TimestampUtil::bpv6_creation_timestamp_t ts;
        bool operator<(const Bpv6Id &o) const;
    };

    struct FragmentInfo {
        std::list<BundleViewV6> bundles;
        FragmentSet::data_fragment_set_t fragmentSet;
    };

    std::map<Bpv6Id, FragmentInfo> m_idToFrags;
    boost::mutex m_mutex;
};

#endif
