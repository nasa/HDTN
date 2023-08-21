#include "Masker.h"

// Subclass Implementation Includes
#include "RedundantMasker.h"
#include "ShiftingMasker.h"

#define MASKER_IMPLEMENTATION_CLASS RedundantMasker

namespace hdtn {

std::shared_ptr<Masker> Masker::makePointer(std::string impl) {
    if (impl == "redundant") {
        return std::make_shared<RedundantMasker>();
    }
    else if (impl == "shifting") {
        return std::make_shared<ShiftingMasker>();
    }
    else {
        return std::make_shared<MASKER_IMPLEMENTATION_CLASS>();
    }
}
}