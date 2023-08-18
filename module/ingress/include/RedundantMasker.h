#include "Masker.h"

#ifndef _RMASKER_H
#define _RMASKER_H 1

namespace hdtn {

class RedundantMasker : public Masker {
public:
	virtual cbhe_eid_t query(const BundleViewV7&) override;
	virtual cbhe_eid_t query(const BundleViewV6&) override;
};

}

#endif