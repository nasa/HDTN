#include "Leider.h"

namespace hdtn {

class RedundantLeider : public Leider {
public:
	virtual cbhe_eid_t RedundantLeider::query(const BundleViewV7&) override;
	virtual cbhe_eid_t RedundantLeider::query(const BundleViewV6&) override;
};

}