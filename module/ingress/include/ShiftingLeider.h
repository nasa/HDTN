#include "Leider.h"

#ifndef _SLEIDER_H
#define _SLEIDER_H 1

namespace hdtn {

class ShiftingLeider : public Leider {
public:
	virtual cbhe_eid_t query(const BundleViewV7&) override;
	virtual cbhe_eid_t query(const BundleViewV6&) override;
};

}

#endif