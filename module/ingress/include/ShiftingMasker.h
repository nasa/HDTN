#ifndef _SMASKER_H
#define _SMASKER_H 1

#include "Masker.h"

namespace hdtn {

class ShiftingMasker : public Masker {
public:
	virtual cbhe_eid_t query(const BundleViewV7&) override;
	virtual cbhe_eid_t query(const BundleViewV6&) override;
};

}

#endif