
#ifndef BPV6_FRAGMENT_H
#define BPV6_FRAGMENT_H

#include "BundleViewV6.h"
#include <list>

/** Fragments bundle into fragments a, b, with a having size sz */
bool fragment(BundleViewV6& orig, uint64_t sz, std::list<BundleViewV6> & fragments);
bool AssembleFragments(std::list<BundleViewV6>& fragments, BundleViewV6& bundle);


#endif
