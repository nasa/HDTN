#ifndef LTP_NOTICES_TO_CLIENT_SERVICE_H
#define LTP_NOTICES_TO_CLIENT_SERVICE_H 1

#include "Ltp.h"
#include <vector>
#include <boost/function.hpp>

//7.1.  Session Start
//The Session Start notice returns the session ID identifying a newly
//created session.
//
//At the sender, the session start notice informs the client service of
//the initiation of the transmission session.On receiving this notice
//the client service may, for example, release resources of its own
//that are allocated to the block being transmitted, or remember the
//session ID so that the session can be canceled in the future if
//necessary.At the receiver, this notice indicates the beginning of a
//new reception session, and is delivered upon arrival of the first
//data segment carrying a new session ID.
typedef boost::function<void(const Ltp::session_id_t & sessionId)> SessionStartCallback_t;

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

//7.4.Transmission - Session Completion
//The sole parameter provided by the LTP engine when a transmission -
//session completion notice is delivered is the session ID of the
//transmission session.
//
//A transmission-session completion notice informs the client service
//that all bytes of the indicated data block have been transmitted and
//that the receiver has received the red - part of the block.
typedef boost::function<void(const Ltp::session_id_t & sessionId)> TransmissionSessionCompletedCallback_t;

//7.5.  Transmission-Session Cancellation
//
//The parameters provided by the LTP engine when a transmission - session
//cancellation notice is delivered are :
//
//Session ID of the transmission session.
//
//The reason - code sent or received in the Cx segment that initiated
//the cancellation sequence.
//
//A transmission-session cancellation notice informs the client service
//that the indicated session was terminated, either by the receiver or
//else due to an error or a resource quench condition in the local LTP
//engine.There is no assurance that the destination client service
//instance received any portion of the data block.
typedef boost::function<void(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode)> TransmissionSessionCancelledCallback_t;

//7.6.  Reception-Session Cancellation
//The parameters provided by the LTP engine when a reception
//cancellation notice is delivered are :
//
//Session ID of the transmission session.
//
//The reason - code explaining the cancellation.
//A reception-session cancellation notice informs the client service
//that the indicated session was terminated, either by the sender or
//else due to an error or a resource quench condition in the local LTP
//engine.No subsequent delivery notices will be issued for this
//session.
typedef boost::function<void(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode)> ReceptionSessionCancelledCallback_t;

//7.7.  Initial-Transmission Completion
//The session ID of the transmission session is included with the
//initial - transmission completion notice.
//
//This notice informs the client service that all segments of a block
//(both red - part and green - part) have been transmitted.This notice
//only indicates that original transmission is complete; retransmission
//of any lost red - part data segments may still be necessary.
typedef boost::function<void(const Ltp::session_id_t & sessionId)> InitialTransmissionCompletedCallback_t;

#endif // LTP_NOTICES_TO_CLIENT_SERVICE_H

