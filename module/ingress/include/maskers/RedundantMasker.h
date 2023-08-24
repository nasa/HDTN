#ifndef _RMASKER_H
#define _RMASKER_H 1

#include "Masker.h"

namespace hdtn {

class RedundantMasker : public Masker {
public:
	virtual ~RedundantMasker() override;
	virtual cbhe_eid_t query(const BundleViewV7&) override;
	virtual cbhe_eid_t query(const BundleViewV6&) override;
};

}

#endif