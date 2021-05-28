#include <boost/test/unit_test.hpp>
#include "LtpEngine.h"
#include <boost/bind.hpp>

BOOST_AUTO_TEST_CASE(LtpEngineTestCase, *boost::unit_test::enabled())
{
    struct Test {
        const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME;
        const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME;
        const uint64_t ENGINE_ID_SRC;
        const uint64_t ENGINE_ID_DEST;
        const uint64_t CLIENT_SERVICE_ID_DEST;
        LtpEngine engineSrc; 
        LtpEngine engineDest;
        const std::string DESIRED_RED_DATA_TO_SEND;
        const std::string DESIRED_RED_AND_GREEN_DATA_TO_SEND;
        const std::string DESIRED_FULLY_GREEN_DATA_TO_SEND;

        uint64_t numRedPartReceptionCallbacks;
        uint64_t numSessionStartSenderCallbacks;
        uint64_t numSessionStartReceiverCallbacks;
        uint64_t numGreenPartReceptionCallbacks;
        uint64_t numReceptionSessionCancelledCallbacks;
        uint64_t numTransmissionSessionCompletedCallbacks;
        uint64_t numInitialTransmissionCompletedCallbacks;
        uint64_t numTransmissionSessionCancelledCallbacks;
        uint64_t numSrcToDestDataExchanged;
        uint64_t numDestToSrcDataExchanged;
        CANCEL_SEGMENT_REASON_CODES lastRxCancelSegmentReasonCode;
        CANCEL_SEGMENT_REASON_CODES lastTxCancelSegmentReasonCode;
        Ltp::session_id_t sessionIdFromSessionStartSender;

        Test() :
            ONE_WAY_LIGHT_TIME(boost::posix_time::seconds(10)),
            ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(2000)),
            ENGINE_ID_SRC(100),
            ENGINE_ID_DEST(200),
            CLIENT_SERVICE_ID_DEST(300),
            engineSrc(ENGINE_ID_SRC, 1, UINT64_MAX, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME),//1=> 1 CHARACTER AT A TIME, UINT64_MAX=> unlimited report segment size
            engineDest(ENGINE_ID_DEST, 1, UINT64_MAX, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME),//1=> MTU NOT USED AT THIS TIME, UINT64_MAX=> unlimited report segment size
            DESIRED_RED_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!"),
            DESIRED_RED_AND_GREEN_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!GGE"), //G=>green data not EOB, E=>green datat EOB
            DESIRED_FULLY_GREEN_DATA_TO_SEND("GGGGGGGGGGGGGGGGGE")
        {
            engineDest.SetSessionStartCallback(boost::bind(&Test::SessionStartReceiverCallback, this, boost::placeholders::_1));
            engineDest.SetRedPartReceptionCallback(boost::bind(&Test::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            engineDest.SetGreenPartSegmentArrivalCallback(boost::bind(&Test::GreenPartSegmentArrivalCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));

            engineDest.SetReceptionSessionCancelledCallback(boost::bind(&Test::ReceptionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));
            engineSrc.SetSessionStartCallback(boost::bind(&Test::SessionStartSenderCallback, this, boost::placeholders::_1));
            engineSrc.SetTransmissionSessionCompletedCallback(boost::bind(&Test::TransmissionSessionCompletedCallback, this, boost::placeholders::_1));
            engineSrc.SetInitialTransmissionCompletedCallback(boost::bind(&Test::InitialTransmissionCompletedCallback, this, boost::placeholders::_1));
            engineSrc.SetTransmissionSessionCancelledCallback(boost::bind(&Test::TransmissionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));
        }

        void SessionStartSenderCallback(const Ltp::session_id_t & sessionId) {
            ++numSessionStartSenderCallbacks;
            sessionIdFromSessionStartSender = sessionId;
        }
        void SessionStartReceiverCallback(const Ltp::session_id_t & sessionId) {
            ++numSessionStartReceiverCallbacks;
            BOOST_REQUIRE(sessionId == sessionIdFromSessionStartSender);
        }
        void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
            std::string receivedMessage(movableClientServiceDataVec.data(), movableClientServiceDataVec.data() + movableClientServiceDataVec.size());
            ++numRedPartReceptionCallbacks;
            BOOST_REQUIRE_EQUAL(receivedMessage, DESIRED_RED_DATA_TO_SEND);
            BOOST_REQUIRE(sessionId == sessionIdFromSessionStartSender);
            //std::cout << "receivedMessage: " << receivedMessage << std::endl;
            //std::cout << "here\n";
        }
        void GreenPartSegmentArrivalCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec, uint64_t offsetStartOfBlock, uint64_t clientServiceId, bool isEndOfBlock) {
            ++numGreenPartReceptionCallbacks;
            BOOST_REQUIRE_EQUAL(movableClientServiceDataVec.size(), 1);
            if (isEndOfBlock) {
                BOOST_REQUIRE_EQUAL(movableClientServiceDataVec[0], 'E');
            }
            else {
                BOOST_REQUIRE_EQUAL(movableClientServiceDataVec[0], 'G');
            }
            BOOST_REQUIRE(sessionId == sessionIdFromSessionStartSender);
        }
        void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
            lastRxCancelSegmentReasonCode = reasonCode;
            ++numReceptionSessionCancelledCallbacks;
            BOOST_REQUIRE(sessionId == sessionIdFromSessionStartSender);
        }
        void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId) {
            ++numTransmissionSessionCompletedCallbacks;
            BOOST_REQUIRE(sessionId == sessionIdFromSessionStartSender);
        }
        void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId) {
            ++numInitialTransmissionCompletedCallbacks;
            BOOST_REQUIRE(sessionId == sessionIdFromSessionStartSender);
        }
        void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
            lastTxCancelSegmentReasonCode = reasonCode;
            ++numTransmissionSessionCancelledCallbacks;
            BOOST_REQUIRE(sessionId == sessionIdFromSessionStartSender);
        }

        static bool SendData(LtpEngine & src, LtpEngine & dest, bool simulateDrop = false, bool swapHeader = false, LTP_SEGMENT_TYPE_FLAGS headerReplacement = LTP_SEGMENT_TYPE_FLAGS::REDDATA) {
            std::vector<boost::asio::const_buffer> constBufferVec;
            boost::shared_ptr<std::vector<std::vector<uint8_t> > >  underlyingDataToDeleteOnSentCallback;
            if (src.NextPacketToSendRoundRobin(constBufferVec, underlyingDataToDeleteOnSentCallback)) {
                if (swapHeader) {
                    uint8_t *data = static_cast<uint8_t*>(const_cast<void*>(constBufferVec[0].data()));
                    data[0] = static_cast<uint8_t>(headerReplacement);
                }
                if (!simulateDrop) {
                    dest.PacketIn(constBufferVec);
                }
                return true;
            }
            return false;
        }

        void Reset() {
            engineSrc.Reset();
            engineDest.Reset();
            numSrcToDestDataExchanged = 0;
            numDestToSrcDataExchanged = 0;
            numRedPartReceptionCallbacks = 0;
            numSessionStartSenderCallbacks = 0;
            numSessionStartReceiverCallbacks = 0;
            numGreenPartReceptionCallbacks = 0;
            numReceptionSessionCancelledCallbacks = 0;
            numTransmissionSessionCompletedCallbacks = 0;
            numInitialTransmissionCompletedCallbacks = 0;
            numTransmissionSessionCancelledCallbacks = 0;
            sessionIdFromSessionStartSender = 0; //sets all fields to 0
        }

        //returns false when no data exchanged
        bool ExchangeData(bool simulateDropSrcToDest = false, bool simulateDropDestToSrc = false, bool swapHeaderSrcToDest = false, bool swapHeaderDestToSrc = false, LTP_SEGMENT_TYPE_FLAGS headerReplacement = LTP_SEGMENT_TYPE_FLAGS::REDDATA) {
            bool didSrcToDest = SendData(engineSrc, engineDest, simulateDropSrcToDest, swapHeaderSrcToDest, headerReplacement);
            bool didDestToSrc = SendData(engineDest, engineSrc, simulateDropDestToSrc, swapHeaderDestToSrc, headerReplacement);
            numSrcToDestDataExchanged += didSrcToDest;
            numDestToSrcDataExchanged += didDestToSrc;
            return (didSrcToDest || didDestToSrc);
        }
        void AssertNoActiveSendersAndReceivers() {
            BOOST_REQUIRE_EQUAL(engineSrc.NumActiveSenders(), 0);
            BOOST_REQUIRE_EQUAL(engineSrc.NumActiveReceivers(), 0);
            BOOST_REQUIRE_EQUAL(engineDest.NumActiveSenders(), 0);
            BOOST_REQUIRE_EQUAL(engineDest.NumActiveReceivers(), 0);
        }
        void AssertOneActiveSenderOnly() {
            BOOST_REQUIRE_EQUAL(engineSrc.NumActiveSenders(), 1);
            BOOST_REQUIRE_EQUAL(engineSrc.NumActiveReceivers(), 0);
            BOOST_REQUIRE_EQUAL(engineDest.NumActiveSenders(), 0);
            BOOST_REQUIRE_EQUAL(engineDest.NumActiveReceivers(), 0);
        }
        void DoTest() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            while (ExchangeData()) {
                
            }
            AssertNoActiveSendersAndReceivers();

            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestOneDropSrcToDest() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            unsigned int count = 0;
            while (ExchangeData(count == 10, false)) {
                ++count;
            }
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 3); //+3 for 2 Report acks and 1 resend
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestTwoDropsSrcToDest() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            unsigned int count = 0;
            while (ExchangeData((count == 10) || (count == 13), false)) {
                ++count;
            }
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 4); //+4 for 2 Report acks and 2 resends
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestTwoDropsSrcToDestRegularCheckpoints() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.SetCheckpointEveryNthDataPacketForSenders(5);
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            unsigned int count = 0;
            while (ExchangeData((count == 2) || (count == 12), false)) { //changed from 7 and 13.. need to guess at the counts so that checkpoints don't end up dropped and stuck in timer which this unit test can't empty callbacks
                ++count;
            }
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 12); //+12 for 10 Report acks (see next line) and 2 resends
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 10); // 44/5=8 + (1 eobCp at 44) + 1 retrans report 
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }
        void DoTestTwoDropsSrcToDestRegularCheckpointsCpBoundary() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.SetCheckpointEveryNthDataPacketForSenders(5);
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            unsigned int count = 0;
            while (ExchangeData((count == 8) || (count == 16), false)) {
                ++count;
            }
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 13); //+13 for 11 Report acks (see next line) and 2 resends
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 11); // 44/5=8 + (1 eobCp at 44) + 1 retrans report TODO not sure why 11 yet
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestRedAndGreenData() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_AND_GREEN_DATA_TO_SEND.data(), DESIRED_RED_AND_GREEN_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            while (ExchangeData()) {

            }
            AssertNoActiveSendersAndReceivers();

            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_AND_GREEN_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 3);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestFullyGreenData() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_FULLY_GREEN_DATA_TO_SEND.data(), DESIRED_FULLY_GREEN_DATA_TO_SEND.size(), 0);
            AssertOneActiveSenderOnly();
            while (ExchangeData()) {

            }
            AssertNoActiveSendersAndReceivers();

            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 0);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestMiscoloredRed() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_FULLY_GREEN_DATA_TO_SEND.data(), DESIRED_FULLY_GREEN_DATA_TO_SEND.size(), DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            unsigned int count = 0;
            while (ExchangeData(false,false, count == 2, false, LTP_SEGMENT_TYPE_FLAGS::GREENDATA)) { //red, red, green, red should trigger miscolored
                ++count;
            }
            AssertNoActiveSendersAndReceivers();

            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, 4+1); //+1 for cancel segment to reach
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 1); //1 for cancel segment
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastRxCancelSegmentReasonCode == CANCEL_SEGMENT_REASON_CODES::MISCOLORED);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastTxCancelSegmentReasonCode == CANCEL_SEGMENT_REASON_CODES::MISCOLORED);
        }

        void DoTestMiscoloredGreen() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            unsigned int count = 0;
            while (ExchangeData((count >= 2) && (count <=10), false, count > (DESIRED_RED_DATA_TO_SEND.size()+3), false, LTP_SEGMENT_TYPE_FLAGS::GREENDATA)) { //red..red repeat red..green
                ++count;
            }
            AssertNoActiveSendersAndReceivers();

            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            //BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 6); //+1 for cancel segment to reach
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 2); //1 rs and 1 cancel segment
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastRxCancelSegmentReasonCode == CANCEL_SEGMENT_REASON_CODES::MISCOLORED);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastTxCancelSegmentReasonCode == CANCEL_SEGMENT_REASON_CODES::MISCOLORED);
        }
    };

    Test t;
    t.DoTest();
    t.DoTestOneDropSrcToDest();
    t.DoTestTwoDropsSrcToDest();
    t.DoTestTwoDropsSrcToDestRegularCheckpoints();
    t.DoTestTwoDropsSrcToDestRegularCheckpointsCpBoundary();
    t.DoTestRedAndGreenData();
    t.DoTestFullyGreenData();
    t.DoTestMiscoloredRed();
    t.DoTestMiscoloredGreen();
}
