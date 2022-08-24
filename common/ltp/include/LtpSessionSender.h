/**
 * @file LtpSessionSender.h
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
 * This LtpSessionSender class encapsulates one LTP sending session.
 * It uses its own asynchronous timer which uses/shares the user's provided boost::asio::io_service.
 */

#ifndef LTP_SESSION_SENDER_H
#define LTP_SESSION_SENDER_H 1

#include "LtpFragmentSet.h"
#include "Ltp.h"
#include "LtpRandomNumberGenerator.h"
#include <queue>
#include <boost/asio.hpp>
#include <memory>
#include "LtpTimerManager.h"
#include "LtpNoticesToClientService.h"
#include "LtpClientServiceDataToSend.h"





typedef boost::function<void(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr)> NotifyEngineThatThisSenderNeedsDeletedCallback_t;

typedef boost::function<void(const uint64_t sessionNumber)> NotifyEngineThatThisSenderHasProducibleDataFunction_t;

class LtpSessionSender {
private:
    LtpSessionSender();
    void LtpCheckpointTimerExpiredCallback(uint64_t checkpointSerialNumber, std::vector<uint8_t> & userData);
public:
    struct LTP_LIB_EXPORT resend_fragment_t {
        resend_fragment_t() {}
        resend_fragment_t(uint64_t paramOffset, uint64_t paramLength, uint64_t paramCheckpointSerialNumber, uint64_t paramReportSerialNumber, LTP_DATA_SEGMENT_TYPE_FLAGS paramFlags) :
            offset(paramOffset), length(paramLength), checkpointSerialNumber(paramCheckpointSerialNumber), reportSerialNumber(paramReportSerialNumber), flags(paramFlags), retryCount(1) {}
        uint64_t offset;
        uint64_t length;
        uint64_t checkpointSerialNumber;
        uint64_t reportSerialNumber;
        LTP_DATA_SEGMENT_TYPE_FLAGS flags;
        uint8_t retryCount;
    };
    LTP_LIB_EXPORT ~LtpSessionSender();
    LTP_LIB_EXPORT LtpSessionSender(uint64_t randomInitialSenderCheckpointSerialNumber, LtpClientServiceDataToSend && dataToSend,
        std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake, uint64_t lengthOfRedPart, const uint64_t MTU,
        const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, boost::asio::io_service & ioServiceRef,
        const NotifyEngineThatThisSenderNeedsDeletedCallback_t & notifyEngineThatThisSenderNeedsDeletedCallback,
        const NotifyEngineThatThisSenderHasProducibleDataFunction_t & notifyEngineThatThisSenderHasProducibleDataFunction,
        const InitialTransmissionCompletedCallback_t & initialTransmissionCompletedCallback,
        const uint64_t checkpointEveryNthDataPacket = 0, const uint32_t maxRetriesPerSerialNumber = 5);
    LTP_LIB_EXPORT bool NextDataToSend(std::vector<boost::asio::const_buffer> & constBufferVec,
        std::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback,
        std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback);
    

    
    LTP_LIB_EXPORT void ReportSegmentReceivedCallback(const Ltp::report_segment_t & reportSegment,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    
private:
    std::set<LtpFragmentSet::data_fragment_t> m_dataFragmentsAckedByReceiver;
    std::queue<std::vector<uint8_t> > m_nonDataToSend;
    std::queue<resend_fragment_t> m_resendFragmentsQueue;
    std::set<uint64_t> m_reportSegmentSerialNumbersReceivedSet;
    LtpTimerManager<uint64_t> m_timeManagerOfCheckpointSerialNumbers;
    uint64_t m_receptionClaimIndex;
    uint64_t m_nextCheckpointSerialNumber;
    std::shared_ptr<LtpClientServiceDataToSend> m_dataToSendSharedPtr;
public:
    std::shared_ptr<LtpTransmissionRequestUserData> m_userDataPtr;
private:
    uint64_t M_LENGTH_OF_RED_PART;
    uint64_t m_dataIndexFirstPass;
    bool m_didNotifyForDeletion;
    const uint64_t M_MTU;
    const Ltp::session_id_t M_SESSION_ID;
    const uint64_t M_CLIENT_SERVICE_ID;
    const uint64_t M_CHECKPOINT_EVERY_NTH_DATA_PACKET;
    uint64_t m_checkpointEveryNthDataPacketCounter;
    const uint32_t M_MAX_RETRIES_PER_SERIAL_NUMBER;
    boost::asio::io_service & m_ioServiceRef;
    const NotifyEngineThatThisSenderNeedsDeletedCallback_t m_notifyEngineThatThisSenderNeedsDeletedCallback;
    const NotifyEngineThatThisSenderHasProducibleDataFunction_t m_notifyEngineThatThisSenderHasProducibleDataFunction;
    const InitialTransmissionCompletedCallback_t m_initialTransmissionCompletedCallback;

public:
    //stats
    uint64_t m_numCheckpointTimerExpiredCallbacks;
    uint64_t m_numDiscretionaryCheckpointsNotResent;
};

#endif // LTP_SESSION_SENDER_H

