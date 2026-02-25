/**
 * @file PrimaryBlock.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
    virtual ~PrimaryBlock() {};
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
