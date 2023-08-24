#include "maskers/ShiftingMasker.h"

// This is arbitrarily defined as 100 by default in order to use this class in a specific demonstration scenario.
// See tests/test_scripts_linux/masker_test_01.sh.
#define SHIFT_NUM 100

namespace hdtn {

cbhe_eid_t ShiftingMasker::query(const BundleViewV7& bv) {
	cbhe_eid_t actual = bv.m_primaryBlockView.header.GetFinalDestinationEid();
	return cbhe_eid_t(actual.nodeId + SHIFT_NUM, actual.serviceId);
}

cbhe_eid_t ShiftingMasker::query(const BundleViewV6& bv) {
	cbhe_eid_t actual = bv.m_primaryBlockView.header.GetFinalDestinationEid();
	return cbhe_eid_t(actual.nodeId + SHIFT_NUM, actual.serviceId);
}

}