#include "Masker.h"

// Subclass Implementation Includes
#include "RedundantMasker.h"
#include "ShiftingMasker.h"

#define DEFAULT_MASKER_IMPLEMENTATION RedundantMasker

namespace hdtn {

Masker::~Masker() {}

std::shared_ptr<Masker> Masker::makePointer(const std::string& impl) {
    if (impl == "redundant") {
        return std::make_shared<RedundantMasker>();
    }
    else if (impl == "shifting") {
        return std::make_shared<ShiftingMasker>();
    }
    else {
        return std::make_shared<DEFAULT_MASKER_IMPLEMENTATION>();
    }
}
}
