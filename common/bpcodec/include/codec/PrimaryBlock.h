#ifndef PRIMARY_BLOCK_H
#define PRIMARY_BLOCK_H 1
#include <cstdint>
#include <cstddef>
#include "Cbhe.h"



struct PrimaryBlock {
    virtual bool HasCustodyFlagSet() const = 0;
    virtual bool HasFragmentationFlagSet() const = 0;
    virtual cbhe_bundle_uuid_t GetCbheBundleUuidFromPrimary() const = 0;
    virtual cbhe_bundle_uuid_nofragment_t GetCbheBundleUuidNoFragmentFromPrimary() const = 0;
    virtual cbhe_eid_t GetFinalDestinationEid() const = 0;
    virtual uint8_t GetPriority() const = 0;
    virtual uint64_t GetExpirationSeconds() const = 0;
    virtual uint64_t GetSequenceForSecondsScale() const = 0;
    virtual uint64_t GetExpirationMilliseconds() const = 0;
    virtual uint64_t GetSequenceForMillisecondsScale() const = 0;
};



#endif //PRIMARY_BLOCK_H
