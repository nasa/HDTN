#include "maskers/ShiftingMasker.h"

namespace hdtn {

cbhe_eid_t ShiftingMasker::query(const BundleViewV7& bv) {
	cbhe_eid_t actual = bv.m_primaryBlockView.header.GetFinalDestinationEid();
	return cbhe_eid_t(actual.nodeId + 100, actual.serviceId);
}

cbhe_eid_t ShiftingMasker::query(const BundleViewV6& bv) {
	cbhe_eid_t actual = bv.m_primaryBlockView.header.GetFinalDestinationEid();
	return cbhe_eid_t(actual.nodeId + 100, actual.serviceId);
}

}