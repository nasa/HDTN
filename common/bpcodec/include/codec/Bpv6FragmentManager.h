/**
 * @file Bpv6FragmentManager.h
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
 * Manager for fragmented bundles. Holds fragments until enough are
 * present to build assembled bundle. Returns assembled bundle.
 */
#ifndef BPV6_FRAGMENT_MANAGER_H
#define BPV6_FRAGMENT_MANAGER_H

#include "BundleViewV6.h"
#include "FragmentSet.h"
#include <list>
#include <boost/thread/mutex.hpp>

/** Collect fragments and assemble when all are present */
class Bpv6FragmentManager {
public:
    /** Add fragment to collection, if final fragment then return completed bundle
     *
     * @param data              The encoded fragment bundle data
     * @param len               Length in bytes of the encoded fragment bundle data
     * @param isComplete[out]   True if bundle completed by fragment added, false otherwise
     * @param assembledBv[out]  If complete, populated with the assembled bundle
     *
     * If the fragment bundle is successfully added and it completes the bundle, the
     * non-fragmented bundle will be assembled in assembledBv.
     *
     * Upon successfully assembling the non-fragmented bundle, the fragments are deleted
     * from the manager.
     *
     * @returns true on successfully adding the fragment, false on error
     */

    BPCODEC_EXPORT bool AddFragmentAndGetComplete(uint8_t *data, size_t len, bool & isComplete, BundleViewV6 & assembledBv);

    /** Thread safe version of AddFragmentAndGetComplete. Protected by mutex */
    BPCODEC_EXPORT bool AddFragmentAndGetComplete_ThreadSafe(uint8_t *data, size_t len, bool & isComplete, BundleViewV6 & assembledBv);

private:
    struct Bpv6Id {
        cbhe_eid_t src;
        TimestampUtil::bpv6_creation_timestamp_t ts;
        bool operator<(const Bpv6Id &o) const;
    };

    struct FragmentInfo {
        std::list<BundleViewV6> bundles;
        FragmentSet::data_fragment_set_t fragmentSet;
    };

    std::map<Bpv6Id, FragmentInfo> m_idToFrags;
    boost::mutex m_mutex;
};

#endif
