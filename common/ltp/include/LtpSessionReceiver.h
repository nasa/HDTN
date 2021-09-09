#ifndef LTP_SESSION_RECEIVER_H
#define LTP_SESSION_RECEIVER_H 1

#include "LtpFragmentSet.h"
#include "Ltp.h"
#include "LtpRandomNumberGenerator.h"
#include "LtpTimerManager.h"
#include <list>
#include <set>
#include <boost/asio.hpp>
#include "LtpNoticesToClientService.h"

typedef boost::function<void(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode)> NotifyEngineThatThisReceiverNeedsDeletedCallback_t;

typedef boost::function<void()> NotifyEngineThatThisReceiversTimersProducedDataFunction_t;

class LtpSessionReceiver {
private:
    LtpSessionReceiver();

    void LtpReportSegmentTimerExpiredCallback(uint64_t reportSerialNumber, std::vector<uint8_t> & userData);
public:
    
    
    LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber, const uint64_t MAX_RECEPTION_CLAIMS, const uint64_t ESTIMATED_BYTES_TO_RECEIVE,
        const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, boost::asio::io_service & ioServiceRef,
        const NotifyEngineThatThisReceiverNeedsDeletedCallback_t & notifyEngineThatThisReceiverNeedsDeletedCallback,
        const NotifyEngineThatThisReceiversTimersProducedDataFunction_t & notifyEngineThatThisSendersTimersProducedDataFunction,
        const uint32_t maxRetriesPerSerialNumber = 5);

    ~LtpSessionReceiver();
    bool NextDataToSend(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback);
    
    
    void ReportAcknowledgementSegmentReceivedCallback(uint64_t reportSerialNumberBeingAcknowledged,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    void DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
        std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions, const RedPartReceptionCallback_t & redPartReceptionCallback,
        const GreenPartSegmentArrivalCallback_t & greenPartSegmentArrivalCallback);
private:
    std::set<LtpFragmentSet::data_fragment_t> m_receivedDataFragmentsSet;
    std::map<uint64_t, Ltp::report_segment_t> m_mapAllReportSegmentsSent;
    std::map<uint64_t, Ltp::report_segment_t> m_mapPrimaryReportSegmentsSent;
    std::set<LtpFragmentSet::data_fragment_t> m_receivedDataFragmentsThatSenderKnowsAboutSet;
    std::set<uint64_t> m_checkpointSerialNumbersReceivedSet;
    std::list<std::pair<uint64_t, uint8_t> > m_reportSerialNumbersToSendList; //pair<reportSerialNumber, retryCount>
    LtpTimerManager<uint64_t> m_timeManagerOfReportSerialNumbers;
    uint64_t m_nextReportSegmentReportSerialNumber;
    std::vector<uint8_t> m_dataReceivedRed;
    const uint64_t M_MAX_RECEPTION_CLAIMS;
    const uint64_t M_ESTIMATED_BYTES_TO_RECEIVE;
    const Ltp::session_id_t M_SESSION_ID;
    const uint64_t M_CLIENT_SERVICE_ID;
    const uint32_t M_MAX_RETRIES_PER_SERIAL_NUMBER;
    uint64_t m_lengthOfRedPart;
    uint64_t m_lowestGreenOffsetReceived;
    uint64_t m_currentRedLength;
    bool m_didRedPartReceptionCallback;
    bool m_didNotifyForDeletion;
    bool m_receivedEobFromGreenOrRed;
    boost::asio::io_service & m_ioServiceRef;
    const NotifyEngineThatThisReceiverNeedsDeletedCallback_t m_notifyEngineThatThisReceiverNeedsDeletedCallback;
    const NotifyEngineThatThisReceiversTimersProducedDataFunction_t m_notifyEngineThatThisReceiversTimersProducedDataFunction;

public:
    //stats
    uint64_t m_numReportSegmentTimerExpiredCallbacks;
    uint64_t m_numReportSegmentsUnableToBeIssued;
    uint64_t m_numReportSegmentsTooLargeAndNeedingSplit;
    uint64_t m_numReportSegmentsCreatedViaSplit;
};

#endif // LTP_SESSION_RECEIVER_H

