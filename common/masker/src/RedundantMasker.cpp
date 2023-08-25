#include "RedundantMasker.h"

namespace hdtn {

RedundantMasker::~RedundantMasker() {}

cbhe_eid_t RedundantMasker::query(const BundleViewV7& bv) {
	return bv.m_primaryBlockView.header.GetFinalDestinationEid();
}

cbhe_eid_t RedundantMasker::query(const BundleViewV6& bv) {
	return bv.m_primaryBlockView.header.GetFinalDestinationEid();
}

}