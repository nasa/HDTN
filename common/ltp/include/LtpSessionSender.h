#ifndef LTP_SESSION_SENDER_H
#define LTP_SESSION_SENDER_H 1

#include "LtpFragmentMap.h"
#include "Ltp.h"
#include "LtpRandomNumberGenerator.h"
#include <list>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

class LtpSessionSender {
private:
    LtpSessionSender();
public:
    struct resend_fragment_t {
        resend_fragment_t(uint64_t paramOffset, uint64_t paramLength, uint64_t paramCheckpointSerialNumber, uint64_t paramReportSerialNumber, LTP_DATA_SEGMENT_TYPE_FLAGS paramFlags) :
            offset(paramOffset), length(paramLength), checkpointSerialNumber(paramCheckpointSerialNumber), reportSerialNumber(paramReportSerialNumber), flags(paramFlags) {}
        uint64_t offset;
        uint64_t length;
        uint64_t checkpointSerialNumber;
        uint64_t reportSerialNumber;
        LTP_DATA_SEGMENT_TYPE_FLAGS flags;
    };
    
    LtpSessionSender(uint64_t randomInitialSenderCheckpointSerialNumber, std::vector<uint8_t> && dataToSend, uint64_t lengthOfRedPart, const uint64_t MTU,
        const Ltp::session_id_t & sessionId, const uint64_t clientServiceId);
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
    uint64_t m_receptionClaimIndex;
    uint64_t m_nextCheckpointSerialNumber;
    std::vector<uint8_t> m_dataToSend;
    uint64_t M_LENGTH_OF_RED_PART;
    uint64_t m_dataIndexFirstPass;
    const uint64_t M_MTU;
    const Ltp::session_id_t M_SESSION_ID;
    const uint64_t M_CLIENT_SERVICE_ID;
};

#endif // LTP_SESSION_SENDER_H

