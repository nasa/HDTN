/**
 * @file BpSourcePattern.h
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
 * The BpSourcePattern class is a pure virtual base class for any application
 * that needs to implement sending user-defined bundles, either at a defined rate,
 * or as fast as possible, or periodic wait for a response (such as a ping application),
 * or episodic when new data becomes available.
 * The application needs only to override the virtual functions GetNextPayloadLength_Step1
 * and CopyPayload_Step2.  If episodic, such as monitoring a folder for new files to become
 * available, the user will also need to override the virtual function TryWaitForDataAvailable.
 * This class takes an HDTN outduct config with one outduct in the outductVector
 * for sending the bundles.
 * In the event that the outduct is not a bidirectional TCPCL outduct,
 * this class can take an optional HDTN induct config with one induct in the inductVector
 * for automatically receiving custody signals and for receiving echo responses (if this is a ping app).
 */

#ifndef _BP_SOURCE_PATTERN_H
#define _BP_SOURCE_PATTERN_H 1
#include "bp_app_patterns_lib_export.h"
#ifndef CLASS_VISIBILITY_BP_APP_PATTERNS_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_BP_APP_PATTERNS_LIB
#  else
#    define CLASS_VISIBILITY_BP_APP_PATTERNS_LIB BP_APP_PATTERNS_LIB_EXPORT
#  endif
#endif
#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "OutductManager.h"
#include "InductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferManager.h"
#include <queue>
#include <unordered_set>
#include "PaddedVectorUint8.h"

class CLASS_VISIBILITY_BP_APP_PATTERNS_LIB BpSourcePattern {
public:
    BP_APP_PATTERNS_LIB_EXPORT BpSourcePattern();
    BP_APP_PATTERNS_LIB_EXPORT virtual ~BpSourcePattern();
    BP_APP_PATTERNS_LIB_EXPORT void Stop();
    BP_APP_PATTERNS_LIB_EXPORT void Start(OutductsConfig_ptr & outductsConfigPtr, InductsConfig_ptr & inductsConfigPtr,
        const boost::filesystem::path& bpSecConfigFilePath,
        bool custodyTransferUseAcs,
        const cbhe_eid_t & myEid, double bundleRate, const cbhe_eid_t & finalDestEid, const uint64_t myCustodianServiceId,
        const unsigned int bundleSendTimeoutSeconds, const uint64_t bundleLifetimeMilliseconds, const uint64_t bundlePriority,
        const bool requireRxBundleBeforeNextTx = false, const bool forceDisableCustody = false, const bool useBpVersion7 = false,
        const uint64_t claRate = 0);

    uint64_t m_bundleCount;
    uint64_t m_numRfc5050CustodyTransfers;
    uint64_t m_numAcsCustodyTransfers;
    uint64_t m_numAcsPacketsReceived;

    uint64_t m_totalNonAdminRecordBpv6PayloadBytesRx;
    uint64_t m_totalNonAdminRecordBpv6BundleBytesRx;
    uint64_t m_totalNonAdminRecordBpv6BundlesRx;

    uint64_t m_totalNonAdminRecordBpv7PayloadBytesRx;
    uint64_t m_totalNonAdminRecordBpv7BundleBytesRx;
    uint64_t m_totalNonAdminRecordBpv7BundlesRx;

    OutductFinalStats m_outductFinalStats;

protected:
    BP_APP_PATTERNS_LIB_EXPORT virtual bool TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout);
    virtual uint64_t GetNextPayloadLength_Step1() = 0;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) = 0;
    BP_APP_PATTERNS_LIB_EXPORT virtual bool ProcessNonAdminRecordBundlePayload(const uint8_t * data, const uint64_t size);
private:
    BP_APP_PATTERNS_LIB_NO_EXPORT void BpSourcePatternThreadFunc(double bundleRate, const boost::filesystem::path& bpSecConfigFilePath);
    BP_APP_PATTERNS_LIB_NO_EXPORT void WholeRxBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec);
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr);
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtrAboutToBeDeleted);
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnFailedBundleVecSendCallback(padded_vector_uint8_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled);
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid);
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid);

    OutductManager m_outductManager;
    InductManager m_inductManager;
    std::unique_ptr<boost::thread> m_bpSourcePatternThreadPtr;
    volatile bool m_running;
    bool m_useCustodyTransfer;
    bool m_custodyTransferUseAcs;
    bool m_useInductForSendingBundles;
    bool m_useBpVersion7;
    uint64_t m_claRate;
    unsigned int m_bundleSendTimeoutSeconds;
    boost::posix_time::time_duration m_bundleSendTimeoutTimeDuration;
    uint64_t m_bundleLifetimeMilliseconds;
    uint64_t m_bundlePriority;
    cbhe_eid_t m_finalDestinationEid;
    cbhe_eid_t m_myEid;
    uint64_t m_myCustodianServiceId;
    cbhe_eid_t m_myCustodianEid;
    std::string m_myCustodianEidUriString;
    boost::mutex m_mutexCtebSet;
    boost::mutex m_mutexBundleUuidSet;
    FragmentSet::data_fragment_set_t m_outstandingCtebCustodyIdsFragmentSet;
    std::set<cbhe_bundle_uuid_nofragment_t> m_cbheBundleUuidSet;
    bool m_detectedNextCustodianSupportsCteb;
    bool m_requireRxBundleBeforeNextTx;
    volatile bool m_isWaitingForRxBundleBeforeNextTx;
    volatile bool m_linkIsDown;
    boost::mutex m_mutexQueueBundlesThatFailedToSend;
    typedef std::pair<uint64_t, uint64_t> bundleid_payloadsize_pair_t;
    typedef std::pair<padded_vector_uint8_t, bundleid_payloadsize_pair_t> bundle_userdata_pair_t;
    std::queue<bundle_userdata_pair_t> m_queueBundlesThatFailedToSend;
    uint64_t m_nextBundleId;
    boost::mutex m_mutexCurrentlySendingBundleIdSet;
    std::unordered_set<uint64_t> m_currentlySendingBundleIdSet;
    boost::mutex m_waitingForRxBundleBeforeNextTxMutex;
    boost::condition_variable m_waitingForRxBundleBeforeNextTxConditionVariable;
    boost::condition_variable m_cvCurrentlySendingBundleIdSet;
    uint64_t m_tcpclOpportunisticRemoteNodeId;
    Induct * m_tcpclInductPtr;
    //version 7 stuff
    cbhe_eid_t m_lastPreviousNode;
    std::vector<uint64_t> m_hopCounts;
public:
    volatile bool m_allOutductsReady;
};


#endif //_BP_SOURCE_PATTERN_H
