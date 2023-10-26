/**
 * @file PrimaryBlock.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This is a pure virtual base class for defining methods common
 * to both BPV6 and BPV7 primary blocks.
 */

#ifndef PRIMARY_BLOCK_H
#define PRIMARY_BLOCK_H 1
#include <cstdint>
#include <cstddef>
#include "Cbhe.h"



struct PrimaryBlock {
    virtual bool HasCustodyFlagSet() const = 0;
    virtual bool HasFragmentationFlagSet() const = 0;
    virtual cbhe_bundle_uuid_t GetCbheBundleUuidFragmentFromPrimary(uint64_t payloadSizeBytes) const = 0;
    virtual cbhe_bundle_uuid_nofragment_t GetCbheBundleUuidNoFragmentFromPrimary() const = 0;
    virtual cbhe_eid_t GetFinalDestinationEid() const = 0;
    virtual cbhe_eid_t GetSourceEid() const = 0;
    virtual uint8_t GetPriority() const = 0;
    virtual uint64_t GetExpirationSeconds() const = 0;
    virtual uint64_t GetSequenceForSecondsScale() const = 0;
    virtual uint64_t GetExpirationMilliseconds() const = 0;
    virtual uint64_t GetSequenceForMillisecondsScale() const = 0;
};



#endif //PRIMARY_BLOCK_H
