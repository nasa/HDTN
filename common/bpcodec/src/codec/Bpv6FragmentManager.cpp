#include "Bpv6FragmentManager.h"

class Bpv6FragmentManager {
public:
    Bpv6FragmentManager();
    bool AddFragment(BundleViewV6 bv, bool & IsComplete);

private:
    struct Bpv6Id {
        cbhe_eid_t src;
        TimestampUtil::bpv6_creation_timestamp_t ts;
    };

    struct FragmentInfo {
        std::vector<BundleViewV6> bundles;
        FragmentSet::data_fragment_set_t parts;
        uint64_t aduLen;
    };

};

#endif
