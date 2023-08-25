#ifndef _SMASKER_H
#define _SMASKER_H 1

#include "Masker.h"

namespace hdtn {

class ShiftingMasker : public Masker {
public:
	MASKER_LIB_EXPORT ShiftingMasker();
	MASKER_LIB_EXPORT ShiftingMasker(uint64_t shiftNum);
	MASKER_LIB_EXPORT virtual ~ShiftingMasker() override;
	MASKER_LIB_EXPORT virtual cbhe_eid_t query(const BundleViewV7&) override;
	MASKER_LIB_EXPORT virtual cbhe_eid_t query(const BundleViewV6&) override;
private:
	// This is arbitrarily initialized to 100 by default in order to use this class in a specific demonstration scenario.
	// See tests/test_scripts_linux/masker_test_01.sh.
	const uint64_t SHIFT_NUM;
};

}

#endif