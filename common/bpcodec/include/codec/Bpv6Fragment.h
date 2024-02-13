/**
 * @file Bpv6Fragment.h
 * @author  Evan Danish <evan.j.danish@nasa.gov>
 *
 * @copyright Copyright (c) 2023 United States Government as represented by
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
 * Fragment Bpv6Bundles
 */
#ifndef BPV6_FRAGMENT_H
#define BPV6_FRAGMENT_H

#include "BundleViewV6.h"
#include <list>

/** Fragment and assemble Bpv6 bundles */
class Bpv6Fragmenter {
    public:

    /** Fragment bundle into multiple fragments of size sz per RFC5050
     *
     * @param orig              The bundle to fragment
     * @param sz                The max payload size for each fragmetn
     * @param fragments[out]    The fragments if successful
     *
     * @returns true if able to fragment, false otherwise
     *
     * The original bundle is not modified. sz must be greater than
     * zero and smaller than the length of the payload of the original
     * bundle.
     *
     * The payload of the fragmented bundles will be at most sz bytes
     * long. Upon successful fragmentation, there will be at least two
     * fragments. All fragments will have payloads of size sz except
     * for possibly the last fragment.
     *
     * The original bundle must not be dirty.
     */
    BPCODEC_EXPORT static bool Fragment(BundleViewV6& orig, uint64_t sz, std::list<BundleViewV6> & fragments);

    /** Assemble multiple fragments into a non-fragmented bundle
     *
     * @param fragments         The fragments to assemble
     * @param bundle[out]       The assembled bundle
     *
     * @returns true if successfully assembled, false otherwise
     *
     * The fragment bundles are not modified. All fragments must
     * be present (though may possibly overlap). See RFC5050 for details.
     *
     * All fragments must have been fragmented from the same original
     * bundle (allowing for multiple fragmenting sessions).
     *
     */
    BPCODEC_EXPORT static bool Assemble(std::list<BundleViewV6>& fragments, BundleViewV6& bundle);

    /** Calculate number of fragments that would be created by fragmenting at fragmentSize
     *
     * @param payloadSize       Size of the payload
     * @param fragmentSize      Desired size of payload in each fragment
     *
     * @returns number of fragments that would be created if fragmenting at fragmentSize
     *
     * Example: CalcNumFragments(80, 40) -> 2
     *
     */
    BPCODEC_EXPORT static uint64_t CalcNumFragments(uint64_t payloadSize, uint64_t fragmentSize);

};

#endif
