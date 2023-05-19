/**
 * @file BpSecPolicyManager.h
 * @author  Nadia Kortas <nadia.kortas@nasa.gov>
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright  2021 United States Government as represented by
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
 * This BpSecPolicyManager defines the methods for looking up BpSec policies
 * based on bundle fields.
 *
 */


#ifndef BPSEC_POLICY_MANAGER_H
#define BPSEC_POLICY_MANAGER_H 1

#include "codec/Cbhe.h"
#include "codec/bpv7.h"
#include "FragmentSet.h"
#include <map>
#include <vector>
#include <array>
#include "bpsec_export.h"

enum class BPSEC_ROLE {
    SOURCE = 0,
    VERIFIER,
    ACCEPTOR,
    RESERVED_MAX_ROLE_TYPES
};
struct BpSecPolicy {
    BpSecPolicy() : m_doIntegrity(false), m_doConfidentiality(false), m_use12ByteIv(true) {}

    bool m_doIntegrity;
    bool m_doConfidentiality;

    //integrity only variables
    COSE_ALGORITHMS m_integrityVariant;
    BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS m_integrityScopeMask;
    BPV7_CRC_TYPE m_bibCrcType;
    FragmentSet::data_fragment_set_t m_bibBlockTypeTargets;
    std::vector<uint8_t> m_hmacKeyEncryptionKey;
    std::vector<uint8_t> m_hmacKey;
    
    //confidentiality only variables
    COSE_ALGORITHMS m_confidentialityVariant;
    bool m_use12ByteIv;
    BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS m_aadScopeMask;
    BPV7_CRC_TYPE m_bcbCrcType;
    FragmentSet::data_fragment_set_t m_bcbBlockTypeTargets;
    std::vector<uint8_t> m_confidentialityKeyEncryptionKey;
    std::vector<uint8_t> m_dataEncryptionKey;
};
typedef std::shared_ptr<BpSecPolicy> BpSecPolicySharedPtr;
typedef std::array<BpSecPolicySharedPtr, static_cast<std::size_t>(BPSEC_ROLE::RESERVED_MAX_ROLE_TYPES)> BpSecPoliciesByRollArray;
struct BpSecPolicyFilter {
    typedef std::map<uint64_t, BpSecPolicyFilter> map_node_id_to_next_filter_t;
    typedef std::map<cbhe_eid_t, BpSecPolicyFilter> map_eid_to_next_filter_t;

    map_node_id_to_next_filter_t m_nodeIdToNextFilterMap;
    map_eid_to_next_filter_t m_eidToNextFilterMap;
    std::unique_ptr<BpSecPolicyFilter> m_anyEidToNextFilterPtr;
    BpSecPoliciesByRollArray m_policiesByRollArray; //used only by filter leaf node
};
class BpSecPolicyManager {
public:

    /**
    * Creates a new BpSecPolicy.  Eid Uri parameters may be in the following form:
    *   - The fully qualified [node,service] pair.
    *   - The node number only (for wildcard service numbers such as "ipn:2.*").
    *   - The "any eid" (for wildcard all such as "ipn:*.*").
    *
    * @param securitySourceEidUri The uri to match of the security source field of the Abstract Security Block (ASB).
    *                             This field should be "ipn:*.*" for a role of "security source" since the ASB won't exist.
    * @param bundleSourceEidUri The uri to match of the bundle source field of the primary block.
    * @param bundleFinalDestEidUri The uri to match of the bundle destination field of the primary block.
    * @param role The Bpsec role of this policy.
    * @return A pointer of the newly allocated policy that needs modified (does not need deleted, handled internally).  NULL if the policy already exists. 
    */
    BPSEC_EXPORT BpSecPolicy* CreateAndGetNewPolicy(const std::string& securitySourceEidUri,
        const std::string& bundleSourceEidUri, const std::string& bundleFinalDestEidUri, const BPSEC_ROLE role);

    /**
    * Finds an existing BpSecPolicy using the fully-qualified eid fields of the bundle.
    * The EID is matched to the most strictest (most fully qualified) set of rules.
    * Lookup shall be performed by a Cascading lookup order:
    *    - The fully qualified [node,service] pair is looked up first for a match.
    *    - The node number only is looked up second for a match (for wildcard service numbers such as "ipn:2.*").
    *    - The "any destination flag" is looked up third for a match (for wildcard all such as "ipn:*.*").
    *
    * @param securitySourceEid The eid to match of the security source field of the Abstract Security Block (ASB).
    *                             This field is a "don't care" for a role of "security source" if the policy was added properly.
    * @param bundleSourceEid The eid to match of the bundle source field of the primary block.
    * @param bundleFinalDestEid The eid to match of the bundle destination field of the primary block.
    * @param role The Bpsec role of this policy.
    * @return A pointer of the existing policy, or NULL if no policy could be matched.
    */
    BPSEC_EXPORT const BpSecPolicy* FindPolicy(const cbhe_eid_t& securitySourceEid,
        const cbhe_eid_t& bundleSourceEid, const cbhe_eid_t& bundleFinalDestEid, const BPSEC_ROLE role) const;

    BPSEC_EXPORT const BpSecPolicy* FindPolicyWithThreadLocalCacheSupport(const cbhe_eid_t& securitySourceEid,
        const cbhe_eid_t& bundleSourceEid, const cbhe_eid_t& bundleFinalDestEid, const BPSEC_ROLE role, bool& wasCacheHit) const;
private:
    BpSecPolicyFilter m_policyFilterSecuritySource;
};


#endif // BPSEC_POLICY_MANAGER_H

