#include "Masker.h"

// Subclass Implementation Includes
#include "RedundantMasker.h"
#include "ShiftingMasker.h"

#define DEFAULT_MASKER_IMPLEMENTATION RedundantMasker

namespace hdtn {

Masker::~Masker() {}

std::shared_ptr<Masker> Masker::makePointer(const std::string& impl, const HdtnConfig& config, zmq::context_t *ctx, const HdtnDistributedConfig& distributedConfig) {
    // Any Masker requiring an HDTN config should use either config or distributedConfig but not both.
    // Any Masker requiring a ZMQ context should use ctx if it is non-null (i.e. HDTN is running in one process); otherwise,
    // the Masker should create its own ZMQ context if one is needed (i.e. running in distributed mode).
    bool isOneProcess = (ctx == NULL) ? false : true;

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
