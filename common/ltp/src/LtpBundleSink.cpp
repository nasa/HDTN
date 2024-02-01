/**
 * @file LtpBundleSink.cpp
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
 */

#include <boost/bind/bind.hpp>
#include <memory>
#include "LtpBundleSink.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpBundleSink::LtpBundleSink(const LtpWholeBundleReadyCallback_t& ltpWholeBundleReadyCallback, const LtpEngineConfig& ltpRxCfg) :
    m_ltpWholeBundleReadyCallback(ltpWholeBundleReadyCallback),
    m_ltpRxCfg(ltpRxCfg),
    M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID(ltpRxCfg.remoteEngineId),
    m_ltpEnginePtr(NULL),
    //telemetry
    M_CONNECTION_NAME(ltpRxCfg.remoteHostname + ":" + boost::lexical_cast<std::string>(ltpRxCfg.remotePort)
        + " Eng:" + boost::lexical_cast<std::string>(ltpRxCfg.remoteEngineId)),
    M_INPUT_NAME(std::string("*:") + boost::lexical_cast<std::string>(ltpRxCfg.myBoundUdpPort)),
    m_totalBundlesReceived(0),
    m_totalBundleBytesReceived(0)
{}

bool LtpBundleSink::Init() {
    if (!SetLtpEnginePtr()) { //virtual function call
        return false;
    }

    m_ltpEnginePtr->SetRedPartReceptionCallback(boost::bind(&LtpBundleSink::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5));
    m_ltpEnginePtr->SetReceptionSessionCancelledCallback(boost::bind(&LtpBundleSink::ReceptionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));

    return true;
}

LtpBundleSink::~LtpBundleSink() {}



void LtpBundleSink::RedPartReceptionCallback(const Ltp::session_id_t & sessionId, padded_vector_uint8_t & movableClientServiceDataVec,
    uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock)
{
    (void)sessionId;
    (void)lengthOfRedPart;
    (void)clientServiceId;
    (void)isEndOfBlock;
    m_totalBundleBytesReceived.fetch_add(movableClientServiceDataVec.size(), std::memory_order_relaxed);
    m_totalBundlesReceived.fetch_add(1, std::memory_order_relaxed);
    m_ltpWholeBundleReadyCallback(movableClientServiceDataVec);

    //This function is holding up the LtpEngine thread.  Once this red part reception callback exits, the last LTP checkpoint report segment (ack)
    //can be sent to the sending ltp engine to close the session
}


void LtpBundleSink::ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
    LOG_INFO(subprocess) << "remote has cancelled session " << sessionId << " with reason code " << (int)reasonCode;
}

void LtpBundleSink::GetTelemetry(LtpInductConnectionTelemetry_t& telem) const {
    telem.m_connectionName = M_CONNECTION_NAME;
    telem.m_inputName = M_INPUT_NAME;
    if (m_ltpEnginePtr) {
        telem.m_totalBundlesReceived = m_totalBundlesReceived.load(std::memory_order_acquire);
        telem.m_totalBundleBytesReceived = m_totalBundleBytesReceived.load(std::memory_order_acquire);

        telem.m_numReportSegmentTimerExpiredCallbacks = m_ltpEnginePtr->m_numReportSegmentTimerExpiredCallbacksRef.load(std::memory_order_acquire);
        telem.m_numReportSegmentsUnableToBeIssued = m_ltpEnginePtr->m_numReportSegmentsUnableToBeIssuedRef.load(std::memory_order_acquire);
        telem.m_numReportSegmentsTooLargeAndNeedingSplit = m_ltpEnginePtr->m_numReportSegmentsTooLargeAndNeedingSplitRef.load(std::memory_order_acquire);
        telem.m_numReportSegmentsCreatedViaSplit = m_ltpEnginePtr->m_numReportSegmentsCreatedViaSplitRef.load(std::memory_order_acquire);
        telem.m_numGapsFilledByOutOfOrderDataSegments = m_ltpEnginePtr->m_numGapsFilledByOutOfOrderDataSegmentsRef.load(std::memory_order_acquire);
        telem.m_numDelayedFullyClaimedPrimaryReportSegmentsSent = m_ltpEnginePtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef.load(std::memory_order_acquire);
        telem.m_numDelayedFullyClaimedSecondaryReportSegmentsSent = m_ltpEnginePtr->m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef.load(std::memory_order_acquire);
        telem.m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent = m_ltpEnginePtr->m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef.load(std::memory_order_acquire);
        telem.m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent = m_ltpEnginePtr->m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef.load(std::memory_order_acquire);

        telem.m_totalCancelSegmentsStarted = m_ltpEnginePtr->m_totalCancelSegmentsStarted.load(std::memory_order_acquire);
        telem.m_totalCancelSegmentSendRetries = m_ltpEnginePtr->m_totalCancelSegmentSendRetries.load(std::memory_order_acquire);
        telem.m_totalCancelSegmentsFailedToSend = m_ltpEnginePtr->m_totalCancelSegmentsFailedToSend.load(std::memory_order_acquire);
        telem.m_totalCancelSegmentsAcknowledged = m_ltpEnginePtr->m_totalCancelSegmentsAcknowledged.load(std::memory_order_acquire);
        telem.m_numRxSessionsCancelledBySender = m_ltpEnginePtr->m_numRxSessionsCancelledBySender.load(std::memory_order_acquire);
        telem.m_numStagnantRxSessionsDeleted = m_ltpEnginePtr->m_numStagnantRxSessionsDeleted.load(std::memory_order_acquire);

        telem.m_countTxUdpPacketsLimitedByRate = m_ltpEnginePtr->m_countAsyncSendsLimitedByRate.load(std::memory_order_acquire);
        GetTransportLayerSpecificTelem(telem); //virtual function call
    }
}
