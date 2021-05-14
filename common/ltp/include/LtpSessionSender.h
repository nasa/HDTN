#ifndef LTP_SESSION_SENDER_H
#define LTP_SESSION_SENDER_H 1

#include "LtpFragmentMap.h"
#include "Ltp.h"
#include "LtpRandomNumberGenerator.h"
#include <list>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include "LtpTimerManager.h"

//A transmission-session completion notice informs the client service
//that all bytes of the indicated data block have been transmitted and
//that the receiver has received the red - part of the block.
typedef boost::function<void(const Ltp::session_id_t & sessionId)> TransmissionSessionCompletedCallback_t;

//This notice informs the client service that all segments of a block
//(both red - part and green - part) have been transmitted.This notice
//only indicates that original transmission is complete; retransmission
//of any lost red - part data segments may still be necessary.
typedef boost::function<void(const Ltp::session_id_t & sessionId)> InitialTransmissionCompletedCallback_t;

//A transmission-session cancellation notice informs the client service
//that the indicated session was terminated, either by the receiver or
//else due to an error or a resource quench condition in the local LTP
//engine.There is no assurance that the destination client service
//instance received any portion of the data block.
typedef boost::function<void(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode)> TransmissionSessionCancelledCallback_t;

typedef boost::function<void(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode)> NotifyEngineThatThisSenderNeedsDeletedCallback_t;

class LtpSessionSender {
private:
    LtpSessionSender();
    void LtpCheckpointTimerExpiredCallback(uint64_t checkpointSerialNumber, std::vector<uint8_t> & userData);
public:
    struct resend_fragment_t {
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
    
    LtpSessionSender(uint64_t randomInitialSenderCheckpointSerialNumber, std::vector<uint8_t> && dataToSend, uint64_t lengthOfRedPart, const uint64_t MTU,
        const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, boost::asio::io_service & ioServiceRef,
        const NotifyEngineThatThisSenderNeedsDeletedCallback_t & notifyEngineThatThisSenderNeedsDeletedCallback,
        const InitialTransmissionCompletedCallback_t & initialTransmissionCompletedCallback,
        const uint64_t checkpointEveryNthDataPacket = 0);
    bool NextDataToSend(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback);
    

    void CancelSegmentReceivedCallback(CANCEL_SEGMENT_REASON_CODES reasonCode, Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    void CancelAcknowledgementSegmentReceivedCallback(Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    void ReportSegmentReceivedCallback(const Ltp::report_segment_t & reportSegment,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    
private:
    std::set<LtpFragmentMap::data_fragment_t> m_dataFragmentsAckedByReceiver;
    std::list<std::vector<uint8_t> > m_nonDataToSend;
    std::list<resend_fragment_t> m_resendFragmentsList;
    std::set<uint64_t> m_reportSegmentSerialNumbersReceivedSet;
    LtpTimerManager m_timeManagerOfCheckpointSerialNumbers;
    uint64_t m_receptionClaimIndex;
    uint64_t m_nextCheckpointSerialNumber;
    std::vector<uint8_t> m_dataToSend;
    uint64_t M_LENGTH_OF_RED_PART;
    uint64_t m_dataIndexFirstPass;
    const uint64_t M_MTU;
    const Ltp::session_id_t M_SESSION_ID;
    const uint64_t M_CLIENT_SERVICE_ID;
    const uint64_t M_CHECKPOINT_EVERY_NTH_DATA_PACKET;
    uint64_t m_checkpointEveryNthDataPacketCounter;
    boost::asio::io_service & m_ioServiceRef;
    const NotifyEngineThatThisSenderNeedsDeletedCallback_t m_notifyEngineThatThisSenderNeedsDeletedCallback;
    const InitialTransmissionCompletedCallback_t m_initialTransmissionCompletedCallback;
};

#endif // LTP_SESSION_SENDER_H

