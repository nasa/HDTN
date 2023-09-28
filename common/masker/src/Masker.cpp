#include "Masker.h"

// Subclass Implementation Includes
#include "RedundantMasker.h"
#include "ShiftingMasker.h"

#define DEFAULT_MASKER_IMPLEMENTATION RedundantMasker

namespace hdtn {

Masker::~Masker() {}

std::shared_ptr<Masker> Masker::makePointer(const std::string& impl, const HdtnConfig& config, zmq::context_t *oneProcessCtx, const HdtnDistributedConfig& distributedConfig) {
    // Some parameters of this function--namely config, oneProcessCtx, and distributedConfig--are currently unused but may be required for future Masker subclasses.
    // 
    // Any future Masker requiring an HDTN config should use either config or distributedConfig but not both.
    // 
    // Any future Masker requiring a ZMQ context should use ctx if it is non-null (i.e. HDTN is running in one process); otherwise,
    // the Masker should create its own ZMQ context if one is needed (i.e. running in distributed mode).
    // 
    // At some point the ZMQ context should be checked like so:
    //bool isOneProcess = (oneProcesCtx == NULL) ? false : true;
    // either in this file or in the constructor of the subclass. It will probably be safest to check it here and have two different constructors
    // for any Masker requiring ZMQ: a single-process constructor that takes the ctx as an argument and a distributed constructor that does not.
    // Alternatively, you could have a single constructor, pass all arguments from this function to it, and let the constructor check.

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
