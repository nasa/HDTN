#ifndef _RMASKER_H
#define _RMASKER_H 1

#include "Masker.h"

namespace hdtn {

class RedundantMasker : public Masker {
public:
	MASKER_LIB_EXPORT virtual ~RedundantMasker() override;
	MASKER_LIB_EXPORT virtual cbhe_eid_t query(const BundleViewV7&) override;
	MASKER_LIB_EXPORT virtual cbhe_eid_t query(const BundleViewV6&) override;
};

}

#endif