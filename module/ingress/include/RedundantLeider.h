#include "Leider.h"

#ifndef _RLEIDER_H
#define _RLEIDER_H 1

namespace hdtn {

class RedundantLeider : public Leider {
public:
	virtual cbhe_eid_t RedundantLeider::query(const BundleViewV7&) override;
	virtual cbhe_eid_t RedundantLeider::query(const BundleViewV6&) override;
};

}

#endif