#include "RedundantLeider.h"

namespace hdtn {

	cbhe_eid_t RedundantLeider::query(const BundleViewV7& bv) {
		return bv.m_primaryBlockView.header.GetFinalDestinationEid();
	}

	cbhe_eid_t RedundantLeider::query(const BundleViewV6& bv) {
		return bv.m_primaryBlockView.header.GetFinalDestinationEid();
	}

}