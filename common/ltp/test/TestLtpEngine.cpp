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

        uint64_t numRedPartReceptionCallbacks;
        uint64_t numReceptionSessionCancelledCallbacks;
        uint64_t numTransmissionSessionCompletedCallbacks;
        uint64_t numInitialTransmissionCompletedCallbacks;
        uint64_t numTransmissionSessionCancelledCallbacks;
        uint64_t numSrcToDestDataExchanged;
        uint64_t numDestToSrcDataExchanged;

        Test() :
            ONE_WAY_LIGHT_TIME(boost::posix_time::seconds(10)),
            ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(2000)),
            ENGINE_ID_SRC(100),
            ENGINE_ID_DEST(200),
            CLIENT_SERVICE_ID_DEST(300),
            engineSrc(ENGINE_ID_SRC, 1, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME),//1=> 1 CHARACTER AT A TIME
            engineDest(ENGINE_ID_DEST, 1, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME),//1=> MTU NOT USED AT THIS TIME
            DESIRED_RED_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!")
        {
            engineDest.SetRedPartReceptionCallback(boost::bind(&Test::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));

            engineDest.SetReceptionSessionCancelledCallback(boost::bind(&Test::ReceptionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));
            engineSrc.SetTransmissionSessionCompletedCallback(boost::bind(&Test::TransmissionSessionCompletedCallback, this, boost::placeholders::_1));
            engineSrc.SetInitialTransmissionCompletedCallback(boost::bind(&Test::InitialTransmissionCompletedCallback, this, boost::placeholders::_1));
            engineSrc.SetTransmissionSessionCancelledCallback(boost::bind(&Test::TransmissionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));
        }

        void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, const std::vector<uint8_t> & clientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
            std::string receivedMessage(clientServiceDataVec.data(), clientServiceDataVec.data() + clientServiceDataVec.size());
            ++numRedPartReceptionCallbacks;
            BOOST_REQUIRE_EQUAL(receivedMessage, DESIRED_RED_DATA_TO_SEND);
            //std::cout << "receivedMessage: " << receivedMessage << std::endl;
            //std::cout << "here\n";
        }
        void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
            ++numReceptionSessionCancelledCallbacks;
        }
        void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId) {
            ++numTransmissionSessionCompletedCallbacks;
        }
        void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId) {
            ++numInitialTransmissionCompletedCallbacks;
        }
        void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
            ++numTransmissionSessionCancelledCallbacks;
        }

        static bool SendData(LtpEngine & src, LtpEngine & dest, bool simulateDrop = false) {
            std::vector<boost::asio::const_buffer> constBufferVec;
            boost::shared_ptr<std::vector<std::vector<uint8_t> > >  underlyingDataToDeleteOnSentCallback;
            if (src.NextPacketToSendRoundRobin(constBufferVec, underlyingDataToDeleteOnSentCallback)) {
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
            numReceptionSessionCancelledCallbacks = 0;
            numTransmissionSessionCompletedCallbacks = 0;
            numInitialTransmissionCompletedCallbacks = 0;
            numTransmissionSessionCancelledCallbacks = 0;
        }

        //returns false when no data exchanged
        bool ExchangeData(bool simulateDropSrcToDest = false, bool simulateDropDestToSrc = false) {
            bool didSrcToDest = SendData(engineSrc, engineDest, simulateDropSrcToDest);
            bool didDestToSrc = SendData(engineDest, engineSrc, simulateDropDestToSrc);
            numSrcToDestDataExchanged += didSrcToDest;
            numDestToSrcDataExchanged += didDestToSrc;
            return (didSrcToDest || didDestToSrc);
        }
        void DoTest() {
            Reset();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            while (ExchangeData()) {
                
            }
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestOneDropSrcToDest() {
            Reset();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            unsigned int count = 0;
            while (ExchangeData(count == 10, false)) {
                ++count;
            }
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 3); //+3 for 2 Report acks and 1 resend
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestTwoDropsSrcToDest() {
            Reset();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            unsigned int count = 0;
            while (ExchangeData((count == 10) || (count == 13), false)) {
                ++count;
            }
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 4); //+4 for 2 Report acks and 2 resends
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestTwoDropsSrcToDestRegularCheckpoints() {
            Reset();
            engineSrc.SetCheckpointEveryNthDataPacketForSenders(5);
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            unsigned int count = 0;
            while (ExchangeData((count == 7) || (count == 13), false)) {
                ++count;
            }
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 11); //+11 for 9 Report acks (see next line) and 2 resends
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 10); // 44/5=8 + (1 eobCp at 44) + 1 retrans report 
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }
        void DoTestTwoDropsSrcToDestRegularCheckpointsCpBoundary() {
            Reset();
            engineSrc.SetCheckpointEveryNthDataPacketForSenders(5);
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            unsigned int count = 0;
            while (ExchangeData((count == 8) || (count == 16), false)) {
                ++count;
            }
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            //BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 11); //+11 for 9 Report acks (see next line) and 2 resends
            //BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 10); // 44/5=8 + (1 eobCp at 44) + 1 retrans report 
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }
    };

    Test t;
    t.DoTest();
    t.DoTestOneDropSrcToDest();
    t.DoTestTwoDropsSrcToDest();
    t.DoTestTwoDropsSrcToDestRegularCheckpoints();
    t.DoTestTwoDropsSrcToDestRegularCheckpointsCpBoundary();
}
