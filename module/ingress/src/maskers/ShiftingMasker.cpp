#include "maskers/ShiftingMasker.h"

namespace hdtn {

ShiftingMasker::ShiftingMasker() : 
	SHIFT_NUM(100)
{
}

ShiftingMasker::ShiftingMasker(uint64_t shiftNum) : 
	SHIFT_NUM(shiftNum)
{	
}

ShiftingMasker::~ShiftingMasker() {}

cbhe_eid_t ShiftingMasker::query(const BundleViewV7& bv) {
	cbhe_eid_t actual = bv.m_primaryBlockView.header.GetFinalDestinationEid();
	return cbhe_eid_t(actual.nodeId + ShiftingMasker::SHIFT_NUM, actual.serviceId);
}

cbhe_eid_t ShiftingMasker::query(const BundleViewV6& bv) {
	cbhe_eid_t actual = bv.m_primaryBlockView.header.GetFinalDestinationEid();
	return cbhe_eid_t(actual.nodeId + ShiftingMasker::SHIFT_NUM, actual.serviceId);
}

}