#include "maskers/Masker.h"

// Subclass Implementation Includes
#include "maskers/RedundantMasker.h"
#include "maskers/ShiftingMasker.h"

#define MASKER_IMPLEMENTATION_CLASS RedundantMasker

namespace hdtn {

std::shared_ptr<Masker> Masker::makePointer(const std::string& impl) {
    // TODO: abstract this pattern, perhaps using macros or templates.
    // Make it easier to add new classes; editing this conditional will get tedious.
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
