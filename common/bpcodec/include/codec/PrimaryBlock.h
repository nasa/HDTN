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
};



#endif //PRIMARY_BLOCK_H
