/**
 * @file CustodyTransferManager.h
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
 * This is a class to help manage BPV6 custody transfer.
 */

#ifndef CUSTODY_TRANSFER_MANAGER_H
#define CUSTODY_TRANSFER_MANAGER_H 1


#include "codec/bpv6.h"
#include "codec/BundleViewV6.h"
#include <string>
#include <cstdint>
#include <vector>

enum class BPV6_ACS_STATUS_REASON_INDICES : uint8_t {
    SUCCESS__NO_ADDITIONAL_INFORMATION = 0,
    FAIL__REDUNDANT_RECEPTION,
    FAIL__DEPLETED_STORAGE,
    FAIL__DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE,
    FAIL__NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE,
    FAIL__NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE,
    FAIL__BLOCK_UNINTELLIGIBLE,

    NUM_INDICES
};
static constexpr unsigned int NUM_ACS_STATUS_INDICES = static_cast<unsigned int>(BPV6_ACS_STATUS_REASON_INDICES::NUM_INDICES);

struct acs_array_t : public std::array<Bpv6AdministrativeRecordContentAggregateCustodySignal, NUM_ACS_STATUS_INDICES> {
    acs_array_t();
};

class CustodyTransferManager {
private:
    CustodyTransferManager();
public:
    
    BPCODEC_EXPORT CustodyTransferManager(const bool isAcsAware, const uint64_t myCustodianNodeId, const uint64_t myCustodianServiceId);
    BPCODEC_EXPORT ~CustodyTransferManager();

    BPCODEC_EXPORT bool ProcessCustodyOfBundle(BundleViewV6 & bv, bool acceptCustody, const uint64_t custodyId,
        const BPV6_ACS_STATUS_REASON_INDICES statusReasonIndex, BundleViewV6 & custodySignalRfc5050RenderedBundleView);
    BPCODEC_EXPORT void SetCreationAndSequence(uint64_t & creation, uint64_t & sequence);
    BPCODEC_EXPORT bool GenerateCustodySignalBundle(BundleViewV6 & newRenderedBundleView, const Bpv6CbhePrimaryBlock & primaryFromSender, const BPV6_ACS_STATUS_REASON_INDICES statusReasonIndex);
    BPCODEC_EXPORT bool GenerateAllAcsBundlesAndClear(std::list<BundleViewV6> & newAcsRenderedBundleViewList);
    BPCODEC_EXPORT bool GenerateAcsBundle(BundleViewV6 & newAcsRenderedBundleView, const cbhe_eid_t & custodianEid, Bpv6AdministrativeRecordContentAggregateCustodySignal & acsToMove, const bool copyAcsOnly = false);
    BPCODEC_EXPORT bool GenerateAcsBundle(BundleViewV6 & newAcsRenderedBundleView, const cbhe_eid_t & custodianEid, const BPV6_ACS_STATUS_REASON_INDICES statusReasonIndex, const bool copyAcsOnly = false);
    BPCODEC_EXPORT const Bpv6AdministrativeRecordContentAggregateCustodySignal & GetAcsConstRef(const cbhe_eid_t & custodianEid, const BPV6_ACS_STATUS_REASON_INDICES statusReasonIndex);
    BPCODEC_EXPORT uint64_t GetLargestNumberOfFills() const;
    BPCODEC_EXPORT bool GenerateBundleDeletionStatusReport(const Bpv6CbhePrimaryBlock & primaryOfDeleted, BundleViewV6 & statusReport);
private:
    const bool m_isAcsAware;
    const uint64_t m_myCustodianNodeId;
    const uint64_t m_myCustodianServiceId;
    const std::string m_myCtebCreatorCustodianEidString;
    std::map<cbhe_eid_t, acs_array_t> m_mapCustodianToAcsArray;
    uint64_t m_lastCreation;
    uint64_t m_sequence;
    uint64_t m_largestNumberOfFills;
};

#endif // CUSTODY_TRANSFER_MANAGER_H

