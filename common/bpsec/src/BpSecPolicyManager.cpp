/**
 * @file BpSecPolicyManager.cpp
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
 */

#include "BpSecPolicyManager.h"
#include "BPSecManager.h"
#include <boost/make_unique.hpp>
#include "Uri.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static BpSecPolicyFilter* Internal_AddPolicyFilterToThisFilter(const std::string& eidUri, BpSecPolicyFilter& thisPolicyFilter) {
    static const std::string anyUriString("ipn:*.*");
    BpSecPolicyFilter* newFilterPtr;

    if (eidUri == anyUriString) {
        if (!thisPolicyFilter.m_anyEidToNextFilterPtr) {
            thisPolicyFilter.m_anyEidToNextFilterPtr = boost::make_unique<BpSecPolicyFilter>();
        }
        newFilterPtr = thisPolicyFilter.m_anyEidToNextFilterPtr.get();
    }
    else {
        cbhe_eid_t destEid;
        bool serviceNumberIsWildCard;
        if (!Uri::ParseIpnUriString(eidUri, destEid.nodeId, destEid.serviceId, &serviceNumberIsWildCard)) {
            LOG_ERROR(subprocess) << "BpSecPolicyManager: eidUri " << eidUri << " is invalid.";
            return NULL;
        }
        if (serviceNumberIsWildCard) {
            newFilterPtr = &thisPolicyFilter.m_nodeIdToNextFilterMap[destEid.nodeId];
        }
        else { //fully qualified eid
            newFilterPtr = &thisPolicyFilter.m_eidToNextFilterMap[destEid];
        }
    }
    return newFilterPtr;
}
BpSecPolicy* BpSecPolicyManager::CreateAndGetNewPolicy(const std::string& securitySourceEidUri,
    const std::string& bundleSourceEidUri, const std::string& bundleFinalDestEidUri, const BPSEC_ROLE role)
{
    BpSecPolicyFilter* policyFilterBundleSourcePtr = Internal_AddPolicyFilterToThisFilter(securitySourceEidUri, m_policyFilterSecuritySource);
    if (!policyFilterBundleSourcePtr) {
        return NULL;
    }
    BpSecPolicyFilter* policyFilterBundleFinalDestPtr = Internal_AddPolicyFilterToThisFilter(bundleSourceEidUri, *policyFilterBundleSourcePtr);
    if (!policyFilterBundleFinalDestPtr) {
        return NULL;
    }
    BpSecPolicyFilter* policyFilterRollArraysPtr = Internal_AddPolicyFilterToThisFilter(bundleFinalDestEidUri, *policyFilterBundleFinalDestPtr);
    if (!policyFilterRollArraysPtr) {
        return NULL;
    }
    const std::size_t roleIndex = static_cast<std::size_t>(role);
    if (roleIndex >= static_cast<std::size_t>(BPSEC_ROLE::RESERVED_MAX_ROLE_TYPES)) {
        return NULL;
    }
    BpSecPolicySharedPtr& policyPtr = policyFilterRollArraysPtr->m_policiesByRollArray[roleIndex];
    if (policyPtr) {
        return NULL; //already exists
    }
    policyPtr = std::make_shared<BpSecPolicy>();
    return policyPtr.get();
}
/*Policy rule lookup:
When a bundle is being received or generated, a policy rule must be found via the policy lookup function.

Lookup shall be performed by a Cascading lookup order:
        - The fully qualified [node,service] pair is looked up first for a match.
        - The node number only is looked up second for a match (for wildcard service numbers such as "ipn:2.*").
        - The "any destination flag" is looked up third for a match (for wildcard all such as "ipn:*.*").

The function shall take the following parameters:

1.) Security source:
    - "acceptor" or "verifier" role:
        - When a bundle is received, this field is the security source field of the ASB.
    - "source role":
        - When a bundle is received (i.e. being forwarded), this field is this receiving node's node number
        - When a new bundle is being created, this field is this bundle originator's node number
2.) Bundle source: The bundle source field of the primary block.
3.) Bundle final destination: The bundle destination field of the primary block.

"acceptor" or "verifier" should filter by security source field of the ASB
"source creators" should filter by their own node number
"source forwarders" should filter by their own node number and optionally bundle primary source and bundle primary dest

*/
static const BpSecPolicyFilter* Internal_GetPolicyFilterFromThisFilter(const cbhe_eid_t& eid, const BpSecPolicyFilter& thisPolicyFilter) {
    { //The fully qualified [node,service] pair is looked up first for a match.
        BpSecPolicyFilter::map_eid_to_next_filter_t::const_iterator it = thisPolicyFilter.m_eidToNextFilterMap.find(eid);
        if (it != thisPolicyFilter.m_eidToNextFilterMap.cend()) {
            return &(it->second);
        }
    }
    { //The node number only is looked up second for a match (for wildcard service numbers such as "ipn:2.*").
        BpSecPolicyFilter::map_node_id_to_next_filter_t::const_iterator it = thisPolicyFilter.m_nodeIdToNextFilterMap.find(eid.nodeId);
        if (it != thisPolicyFilter.m_nodeIdToNextFilterMap.cend()) {
            return &(it->second);
        }
    }
    //The "any destination flag" is looked up third for a match (for wildcard all such as "ipn:*.*").
    return thisPolicyFilter.m_anyEidToNextFilterPtr.get();

}
const BpSecPolicy* BpSecPolicyManager::FindPolicy(const cbhe_eid_t& securitySourceEid,
    const cbhe_eid_t& bundleSourceEid, const cbhe_eid_t& bundleFinalDestEid, const BPSEC_ROLE role) const
{
    const BpSecPolicyFilter* policyFilterBundleSourcePtr = Internal_GetPolicyFilterFromThisFilter(securitySourceEid, m_policyFilterSecuritySource);
    if (!policyFilterBundleSourcePtr) {
        return NULL;
    }
    const BpSecPolicyFilter* policyFilterBundleFinalDestPtr = Internal_GetPolicyFilterFromThisFilter(bundleSourceEid, *policyFilterBundleSourcePtr);
    if (!policyFilterBundleFinalDestPtr) {
        return NULL;
    }
    const BpSecPolicyFilter* policyFilterRollArraysPtr = Internal_GetPolicyFilterFromThisFilter(bundleFinalDestEid, *policyFilterBundleFinalDestPtr);
    if (!policyFilterRollArraysPtr) {
        return NULL;
    }
    const std::size_t roleIndex = static_cast<std::size_t>(role);
    if (roleIndex >= static_cast<std::size_t>(BPSEC_ROLE::RESERVED_MAX_ROLE_TYPES)) {
        return NULL;
    }
    return policyFilterRollArraysPtr->m_policiesByRollArray[roleIndex].get();
}

const BpSecPolicy* BpSecPolicyManager::FindPolicyWithThreadLocalCacheSupport(const cbhe_eid_t& securitySourceEid,
    const cbhe_eid_t& bundleSourceEid, const cbhe_eid_t& bundleFinalDestEid, const BPSEC_ROLE role, bool& wasCacheHit) const
{
    struct LocalCache {
        LocalCache() : 
            securitySourceEid(0, 0),
            bundleSourceEid(0, 0),
            bundleFinalDestEid(0, 0),
            role(BPSEC_ROLE::RESERVED_MAX_ROLE_TYPES),
            foundPolicy(NULL) {}
        cbhe_eid_t securitySourceEid;
        cbhe_eid_t bundleSourceEid;
        cbhe_eid_t bundleFinalDestEid;
        BPSEC_ROLE role;
        const BpSecPolicy* foundPolicy;
    };
    static thread_local LocalCache localCache;
    wasCacheHit = false;
    if (localCache.foundPolicy && (role == localCache.role)
        && (securitySourceEid == localCache.securitySourceEid)
        && (bundleSourceEid == localCache.bundleSourceEid)
        && (bundleFinalDestEid == localCache.bundleFinalDestEid))
    {
        wasCacheHit = true;
        return localCache.foundPolicy;
    }
    localCache.foundPolicy = BpSecPolicyManager::FindPolicy(securitySourceEid,
        bundleSourceEid, bundleFinalDestEid, role);
    if (localCache.foundPolicy) {
        localCache.role = role;
        localCache.securitySourceEid = securitySourceEid;
        localCache.bundleSourceEid = bundleSourceEid;
        localCache.bundleFinalDestEid = bundleFinalDestEid;
    }
    return localCache.foundPolicy;
}

bool BpSecPolicyManager::ProcessReceivedBundle_ThreadLocal(BundleViewV7& bv) const {
    const Bpv7CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;
    bool wasCacheHit;
    bool decryptionSuccess = false;
    static thread_local BPSecManager::ReusableElementsInternal bpsecReusableElementsInternal;
    static thread_local BPSecManager::HmacCtxWrapper hmacCtxWrapper;
    static thread_local BPSecManager::EvpCipherCtxWrapper evpCtxWrapper;
    static thread_local BPSecManager::EvpCipherCtxWrapper ctxWrapperKeyWrapOps;
    static thread_local std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        BundleViewV7::Bpv7CanonicalBlockView& bcbBlockView = *(blocks[i]);
        Bpv7BlockConfidentialityBlock* bcbPtr = dynamic_cast<Bpv7BlockConfidentialityBlock*>(bcbBlockView.headerPtr.get());
        if (!bcbPtr) {
            LOG_ERROR(subprocess) << "cast to bcb block failed";
            return false;
        }
        const BpSecPolicy* bpSecPolicyPtr = FindPolicyWithThreadLocalCacheSupport(
            bcbPtr->m_securitySource, primary.m_sourceNodeId, primary.m_destinationEid, BPSEC_ROLE::ACCEPTOR, wasCacheHit);
        if (!bpSecPolicyPtr) {
            continue;
        }
        if (!bpSecPolicyPtr->m_doConfidentiality) {
            continue;
        }

        //does not rerender in place here, there are more ops to complete after decryption and then a manual render-in-place will be called later
        if (!BPSecManager::TryDecryptBundleByIndividualBcb(evpCtxWrapper,
            ctxWrapperKeyWrapOps,
            bv,
            bcbBlockView,
            bpSecPolicyPtr->m_confidentialityKeyEncryptionKey.empty() ? NULL : bpSecPolicyPtr->m_confidentialityKeyEncryptionKey.data(), //NULL if not present (for wrapping DEK only)
            static_cast<unsigned int>(bpSecPolicyPtr->m_confidentialityKeyEncryptionKey.size()),
            bpSecPolicyPtr->m_dataEncryptionKey.empty() ? NULL : bpSecPolicyPtr->m_dataEncryptionKey.data(), //NULL if not present (when no wrapped key is present)
            static_cast<unsigned int>(bpSecPolicyPtr->m_dataEncryptionKey.size()),
            bpsecReusableElementsInternal))
        {
            LOG_ERROR(subprocess) << "Process: version 7 bundle received but cannot decrypt";
            return false;
        }
        decryptionSuccess = true;
    }

    if (decryptionSuccess) {
        static thread_local bool printedMsg = false;
        if (!printedMsg) {
            LOG_INFO(subprocess) << "first time decrypted bundle successfully from source node "
                << bv.m_primaryBlockView.header.m_sourceNodeId
                << " ..(This message type will now be suppressed.)";
            printedMsg = true;
        }
    }

    bool integritySuccess = false;
    bool markBibForDeletion = false;
    bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        BundleViewV7::Bpv7CanonicalBlockView& bibBlockView = *(blocks[i]);
        Bpv7BlockIntegrityBlock* bibPtr = dynamic_cast<Bpv7BlockIntegrityBlock*>(bibBlockView.headerPtr.get());
        if (!bibPtr) {
            LOG_ERROR(subprocess) << "cast to bib block failed";
            return false;
        }
        const BpSecPolicy* bpSecPolicyPtr = FindPolicyWithThreadLocalCacheSupport(
            bibPtr->m_securitySource, primary.m_sourceNodeId, primary.m_destinationEid, BPSEC_ROLE::ACCEPTOR, wasCacheHit);

        if (bpSecPolicyPtr) {
            markBibForDeletion = true; //true for acceptors
        }
        else {
            bpSecPolicyPtr = FindPolicyWithThreadLocalCacheSupport(
                bibPtr->m_securitySource, primary.m_sourceNodeId, primary.m_destinationEid, BPSEC_ROLE::VERIFIER, wasCacheHit);
            if (!bpSecPolicyPtr) {
                continue;
            }
            markBibForDeletion = false; //false for verifiers
        }

        if (!bpSecPolicyPtr->m_doIntegrity) {
            continue;
        }

        //does not rerender in place here, there are more ops to complete after decryption and then a manual render-in-place will be called later
        if (!BPSecManager::TryVerifyBundleIntegrityByIndividualBib(hmacCtxWrapper,
            ctxWrapperKeyWrapOps,
            bv,
            bibBlockView,
            bpSecPolicyPtr->m_hmacKeyEncryptionKey.empty() ? NULL : bpSecPolicyPtr->m_hmacKeyEncryptionKey.data(), //NULL if not present (for unwrapping hmac key only)
            static_cast<unsigned int>(bpSecPolicyPtr->m_hmacKeyEncryptionKey.size()),
            bpSecPolicyPtr->m_hmacKey.empty() ? NULL : bpSecPolicyPtr->m_hmacKey.data(), //NULL if not present (when no wrapped key is present)
            static_cast<unsigned int>(bpSecPolicyPtr->m_hmacKey.size()),
            bpsecReusableElementsInternal,
            markBibForDeletion))
        {
            LOG_ERROR(subprocess) << "Process: version 7 bundle received but cannot check integrity";
            return false;
        }
        integritySuccess = true;
    }
    if (integritySuccess) {
        static thread_local bool printedMsg = false;
        if (!printedMsg) {
            LOG_INFO(subprocess) << "first time " << ((markBibForDeletion) ? "accepted" : "verified") << " a bundle's integrity successfully from source node "
                << bv.m_primaryBlockView.header.m_sourceNodeId
                << " ..(This message type will now be suppressed.)";
            printedMsg = true;
        }
    }
    return true;
}
