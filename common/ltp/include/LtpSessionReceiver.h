#ifndef LTP_SESSION_RECEIVER_H
#define LTP_SESSION_RECEIVER_H 1

#include "LtpFragmentMap.h"
#include "Ltp.h"
#include "LtpRandomNumberGenerator.h"
#include "LtpTimerManager.h"
#include <list>
#include <set>
#include <boost/asio.hpp>

//7.2.  Green-Part Segment Arrival
//The following parameters are provided by the LTP engine when a green -
//part segment arrival notice is delivered :
//
//Session ID of the transmission session.
//
//Array of client service data bytes contained in the data segment.
//
//Offset of the data segment's content from the start of the block.
//
//Length of the data segment's content. (shall be included in the vector size() function)
//
//Indication as to whether or not the last byte of this data
//segment's content is also the end of the block.
//
//Source LTP engine ID.
typedef boost::function<void(const Ltp::session_id_t & sessionId,
    std::vector<uint8_t> & movableClientServiceDataVec, uint64_t offsetStartOfBlock,
    uint64_t clientServiceId, bool isEndOfBlock)> GreenPartSegmentArrivalCallback_t;

//7.3.  Red-Part Reception
//The following parameters are provided by the LTP engine when a red -
//part reception notice is delivered :
//Session ID of the transmission session.
//
//Array of client service data bytes that constitute the red - part of
//the block.
//
//Length of the red - part of the block.
//
//Indication as to whether or not the last byte of the red - part is
//also the end of the block.
//
//Source LTP engine ID.
typedef boost::function<void(const Ltp::session_id_t & sessionId,
    std::vector<uint8_t> & movableClientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock)> RedPartReceptionCallback_t;

//A reception-session cancellation notice informs the client service
//that the indicated session was terminated, either by the sender or
//else due to an error or a resource quench condition in the local LTP
//engine.No subsequent delivery notices will be issued for this
//session.
typedef boost::function<void(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode)> ReceptionSessionCancelledCallback_t;

typedef boost::function<void(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode)> NotifyEngineThatThisReceiverNeedsDeletedCallback_t;

typedef boost::function<void()> NotifyEngineThatThisReceiversTimersProducedDataFunction_t;

class LtpSessionReceiver {
private:
    LtpSessionReceiver();

    void LtpReportSegmentTimerExpiredCallback(uint64_t reportSerialNumber, std::vector<uint8_t> & userData);
public:
    
    
    LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber, const uint64_t MTU, const uint64_t ESTIMATED_BYTES_TO_RECEIVE,
        const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, boost::asio::io_service & ioServiceRef,
        const NotifyEngineThatThisReceiverNeedsDeletedCallback_t & notifyEngineThatThisReceiverNeedsDeletedCallback,
        const NotifyEngineThatThisReceiversTimersProducedDataFunction_t & notifyEngineThatThisSendersTimersProducedDataFunction);

    bool NextDataToSend(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback);
    
    
    void ReportAcknowledgementSegmentReceivedCallback(uint64_t reportSerialNumberBeingAcknowledged,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    void DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
        std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions, const RedPartReceptionCallback_t & redPartReceptionCallback,
        const GreenPartSegmentArrivalCallback_t & greenPartSegmentArrivalCallback);
private:
    std::set<LtpFragmentMap::data_fragment_t> m_receivedDataFragmentsSet;
    std::map<uint64_t, Ltp::report_segment_t> m_mapAllReportSegmentsSent;
    std::map<uint64_t, Ltp::report_segment_t> m_mapPrimaryReportSegmentsSent;
    std::set<LtpFragmentMap::data_fragment_t> m_receivedDataFragmentsThatSenderKnowsAboutSet;
    std::set<uint64_t> m_checkpointSerialNumbersReceivedSet;
    std::list<std::pair<uint64_t, uint8_t> > m_reportSerialNumbersToSendList; //pair<reportSerialNumber, retryCount>
    LtpTimerManager<uint64_t> m_timeManagerOfReportSerialNumbers;
    uint64_t m_nextReportSegmentReportSerialNumber;
    std::vector<uint8_t> m_dataReceivedRed;
    const uint64_t M_MTU;
    const uint64_t M_ESTIMATED_BYTES_TO_RECEIVE;
    const Ltp::session_id_t M_SESSION_ID;
    const uint64_t M_CLIENT_SERVICE_ID;
    uint64_t m_lengthOfRedPart;
    uint64_t m_lowestGreenOffsetReceived;
    uint64_t m_highestRedOffsetReceived;
    bool m_didRedPartReceptionCallback;
    bool m_didNotifyForDeletion;
    bool m_receivedEobFromGreenOrRed;
    boost::asio::io_service & m_ioServiceRef;
    const NotifyEngineThatThisReceiverNeedsDeletedCallback_t m_notifyEngineThatThisReceiverNeedsDeletedCallback;
    const NotifyEngineThatThisReceiversTimersProducedDataFunction_t m_notifyEngineThatThisReceiversTimersProducedDataFunction;

public:
    //stats
    uint64_t m_numTimerExpiredCallbacks;
};

#endif // LTP_SESSION_RECEIVER_H

