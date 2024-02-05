/**
 * @file TestLtpEngine.cpp
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

#include <boost/test/unit_test.hpp>
#include "LtpEngine.h"
#include <boost/bind/bind.hpp>
#include <set>

BOOST_AUTO_TEST_CASE(LtpEngineTestCase, *boost::unit_test::enabled())
{
    struct Test {
        const uint8_t engineIndexForEncodingIntoRandomSessionNumber;
        const uint64_t ENGINE_ID_SRC;
        const uint64_t ENGINE_ID_DEST;
        const uint64_t CLIENT_SERVICE_ID_DEST;
        LtpEngine engineSrc; 
        LtpEngine engineDest;
        const std::string DESIRED_RED_DATA_TO_SEND;
        const std::string DESIRED_TOO_MUCH_RED_DATA_TO_SEND;
        const std::string DESIRED_RED_AND_GREEN_DATA_TO_SEND;
        const std::string DESIRED_FULLY_GREEN_DATA_TO_SEND;

        uint64_t numRedPartReceptionCallbacks;
        uint64_t numRedPartReceptionsThatWereEndOfBlock;
        uint64_t numSessionStartSenderCallbacks;
        uint64_t numSessionStartReceiverCallbacks;
        uint64_t numGreenPartReceptionCallbacks;
        std::set<uint64_t> greenPartOffsetsReceivedSet;
        uint64_t numReceptionSessionCancelledCallbacks;
        uint64_t numTransmissionSessionCompletedCallbacks;
        uint64_t numInitialTransmissionCompletedCallbacks;
        uint64_t numTransmissionSessionCancelledCallbacks;
        uint64_t numSrcToDestDataExchanged;
        uint64_t numDestToSrcDataExchanged;
        CANCEL_SEGMENT_REASON_CODES lastRxCancelSegmentReasonCode;
        CANCEL_SEGMENT_REASON_CODES lastTxCancelSegmentReasonCode;
        Ltp::session_id_t sessionIdFromSessionStartSender;

        Test(const LtpEngineConfig& ltpRxCfg, const LtpEngineConfig& ltpTxCfg) :
            engineIndexForEncodingIntoRandomSessionNumber(1),
            ENGINE_ID_SRC(ltpTxCfg.thisEngineId),
            ENGINE_ID_DEST(ltpRxCfg.thisEngineId),
            CLIENT_SERVICE_ID_DEST(ltpRxCfg.clientServiceId),
            //last parameter 1 in the following two constructors (maxUdpPacketsToSendPerSystemCall) is a don't care since m_ioServiceLtpEngineThreadPtr is NULL for this test
            //delaySendingOfReportSegmentsTimeMsOrZeroToDisable must be 0 for this test
            //delaySendingOfDataSegmentsTimeMsOrZeroToDisable must be 0 for this test
            engineSrc(ltpTxCfg, engineIndexForEncodingIntoRandomSessionNumber, false),
            engineDest(ltpRxCfg, engineIndexForEncodingIntoRandomSessionNumber, false),
            DESIRED_RED_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!"),
            DESIRED_TOO_MUCH_RED_DATA_TO_SEND("The quick brown fox jumps over the lazy dog! 12345678910"),
            DESIRED_RED_AND_GREEN_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!GGE"), //G=>green data not EOB, E=>green data EOB
            DESIRED_FULLY_GREEN_DATA_TO_SEND("GGGGGGGGGGGGGGGGGE"),
            numRedPartReceptionCallbacks(0),
            numRedPartReceptionsThatWereEndOfBlock(0),
            numSessionStartSenderCallbacks(0),
            numSessionStartReceiverCallbacks(0),
            numGreenPartReceptionCallbacks(0),
            numReceptionSessionCancelledCallbacks(0),
            numTransmissionSessionCompletedCallbacks(0),
            numInitialTransmissionCompletedCallbacks(0),
            numTransmissionSessionCancelledCallbacks(0),
            numSrcToDestDataExchanged(0),
            numDestToSrcDataExchanged(0),
            lastRxCancelSegmentReasonCode(CANCEL_SEGMENT_REASON_CODES::RESERVED),
            lastTxCancelSegmentReasonCode(CANCEL_SEGMENT_REASON_CODES::RESERVED),
            sessionIdFromSessionStartSender(0, 0)
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
        void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, padded_vector_uint8_t & movableClientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
            std::string receivedMessage(movableClientServiceDataVec.data(), movableClientServiceDataVec.data() + movableClientServiceDataVec.size());
            ++numRedPartReceptionCallbacks;
            numRedPartReceptionsThatWereEndOfBlock += isEndOfBlock;
            BOOST_REQUIRE_EQUAL(receivedMessage, DESIRED_RED_DATA_TO_SEND);
            BOOST_REQUIRE(sessionId == sessionIdFromSessionStartSender);
            BOOST_REQUIRE_EQUAL(lengthOfRedPart, movableClientServiceDataVec.size());
            BOOST_REQUIRE_EQUAL(clientServiceId, CLIENT_SERVICE_ID_DEST);
        }
        void GreenPartSegmentArrivalCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec, uint64_t offsetStartOfBlock, uint64_t clientServiceId, bool isEndOfBlock) {
            ++numGreenPartReceptionCallbacks;
            BOOST_REQUIRE(greenPartOffsetsReceivedSet.emplace(offsetStartOfBlock).second);
            BOOST_REQUIRE_EQUAL(movableClientServiceDataVec.size(), 1);
            if (isEndOfBlock) {
                BOOST_REQUIRE_EQUAL(movableClientServiceDataVec[0], 'E');
            }
            else {
                BOOST_REQUIRE_EQUAL(movableClientServiceDataVec[0], 'G');
            }
            BOOST_REQUIRE(sessionId == sessionIdFromSessionStartSender);
            BOOST_REQUIRE_EQUAL(clientServiceId, CLIENT_SERVICE_ID_DEST);
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
            UdpSendPacketInfo udpSendPacketInfo;
            if (src.GetNextPacketToSend(udpSendPacketInfo)) {
                if (swapHeader) {
                    uint8_t *data = static_cast<uint8_t*>(const_cast<void*>(udpSendPacketInfo.constBufferVec[0].data()));
                    data[0] = static_cast<uint8_t>(headerReplacement);
                }
                if (!simulateDrop) {
                    dest.PacketIn(udpSendPacketInfo.constBufferVec);
                }
                return true;
            }
            return false;
        }

        void Reset() {
            engineSrc.Reset();
            engineDest.Reset();
            engineSrc.SetCheckpointEveryNthDataPacketForSenders(0);
            engineDest.SetCheckpointEveryNthDataPacketForSenders(0);
            numSrcToDestDataExchanged = 0;
            numDestToSrcDataExchanged = 0;
            numRedPartReceptionCallbacks = 0;
            numRedPartReceptionsThatWereEndOfBlock = 0;
            numSessionStartSenderCallbacks = 0;
            numSessionStartReceiverCallbacks = 0;
            numGreenPartReceptionCallbacks = 0;
            greenPartOffsetsReceivedSet.clear();
            numReceptionSessionCancelledCallbacks = 0;
            numTransmissionSessionCompletedCallbacks = 0;
            numInitialTransmissionCompletedCallbacks = 0;
            numTransmissionSessionCancelledCallbacks = 0;
            lastRxCancelSegmentReasonCode = CANCEL_SEGMENT_REASON_CODES::RESERVED;
            lastTxCancelSegmentReasonCode = CANCEL_SEGMENT_REASON_CODES::RESERVED;
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
            BOOST_REQUIRE_EQUAL(numRedPartReceptionsThatWereEndOfBlock, 1);
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
            BOOST_REQUIRE_EQUAL(numRedPartReceptionsThatWereEndOfBlock, 1);
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
            BOOST_REQUIRE_EQUAL(numRedPartReceptionsThatWereEndOfBlock, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestTwoDropsConsecutiveMtuContrainedSrcToDest() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            unsigned int count = 0;
            while (ExchangeData((count == 10) || (count == 11), false)) {
                ++count;
            }
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 4); //+4 for 2 Report acks and 2 resends (2 resends instead of 1 because MTU should constrain)
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionsThatWereEndOfBlock, 1);
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
            BOOST_REQUIRE_EQUAL(numRedPartReceptionsThatWereEndOfBlock, 1);
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
            BOOST_REQUIRE_EQUAL(numRedPartReceptionsThatWereEndOfBlock, 1);
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
            BOOST_REQUIRE_EQUAL(numRedPartReceptionsThatWereEndOfBlock, 0); //0 because there's green data after the red part
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 3);
            BOOST_REQUIRE(greenPartOffsetsReceivedSet == std::set<uint64_t>({
                DESIRED_RED_AND_GREEN_DATA_TO_SEND.length() - 3,
                DESIRED_RED_AND_GREEN_DATA_TO_SEND.length() - 2,
                DESIRED_RED_AND_GREEN_DATA_TO_SEND.length() - 1
            }));
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
            std::set<uint64_t> expectedGreenPartOffsetsReceivedSet;
            for (uint64_t i = 0; i < DESIRED_FULLY_GREEN_DATA_TO_SEND.size(); ++i) {
                expectedGreenPartOffsetsReceivedSet.emplace(i);
            }
            BOOST_REQUIRE_EQUAL(expectedGreenPartOffsetsReceivedSet.size(), DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            BOOST_REQUIRE(greenPartOffsetsReceivedSet == expectedGreenPartOffsetsReceivedSet);
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
            BOOST_REQUIRE(greenPartOffsetsReceivedSet == std::set<uint64_t>({2}));
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

        void DoTestTooMuchRedData() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_TOO_MUCH_RED_DATA_TO_SEND.data(), DESIRED_TOO_MUCH_RED_DATA_TO_SEND.size(), DESIRED_TOO_MUCH_RED_DATA_TO_SEND.size());
            AssertOneActiveSenderOnly();
            while (ExchangeData()) { //red, red, green, red should trigger miscolored

            }
            AssertNoActiveSendersAndReceivers();
            
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastRxCancelSegmentReasonCode == CANCEL_SEGMENT_REASON_CODES::SYSTEM_CANCELLED);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastTxCancelSegmentReasonCode == CANCEL_SEGMENT_REASON_CODES::SYSTEM_CANCELLED);
        }
    };


    const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME(boost::posix_time::seconds(10));
    const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(2000));
    const uint64_t ENGINE_ID_SRC(100);
    const uint64_t ENGINE_ID_DEST(200);
    const uint64_t CLIENT_SERVICE_ID_DEST(300);

    LtpEngineConfig ltpRxCfg;

    ltpRxCfg.thisEngineId = ENGINE_ID_DEST;
    ltpRxCfg.remoteEngineId = ENGINE_ID_SRC; // (not used at the LtpEngine level but at the routing level, so this is "don't care") //expectedSessionOriginatorEngineId to be received
    ltpRxCfg.clientServiceId = CLIENT_SERVICE_ID_DEST; //not currently checked by induct
    ltpRxCfg.isInduct = true;
    ltpRxCfg.mtuClientServiceData = 1; //unused for inducts //1=> MTU NOT USED AT THIS TIME,
    ltpRxCfg.mtuReportSegment = UINT64_MAX; //UINT64_MAX=> unlimited report segment size
    ltpRxCfg.oneWayLightTime = ONE_WAY_LIGHT_TIME;
    ltpRxCfg.oneWayMarginTime = ONE_WAY_MARGIN_TIME;
    //ltpRxCfg.remoteHostname = ;
    //ltpRxCfg.remotePort = ;
    //ltpRxCfg.myBoundUdpPort = ;
    //ltpRxCfg.numUdpRxCircularBufferVectors = ;
    ltpRxCfg.estimatedBytesToReceivePerSession = 0; //force a resize
    ltpRxCfg.maxRedRxBytesPerSession = 50;
    ltpRxCfg.checkpointEveryNthDataPacketSender = 0; //unused for inducts
    ltpRxCfg.maxRetriesPerSerialNumber = 5;
    ltpRxCfg.force32BitRandomNumbers = false;
    ltpRxCfg.maxSendRateBitsPerSecOrZeroToDisable = 0;
    ltpRxCfg.maxSimultaneousSessions = 100;
    ltpRxCfg.rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable = 1000;
    ltpRxCfg.maxUdpPacketsToSendPerSystemCall = 1; //is a don't care since m_ioServiceLtpEngineThreadPtr is NULL for this test
    ltpRxCfg.senderPingSecondsOrZeroToDisable = 0; //unused for inducts
    ltpRxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable = 0; //must be 0 for this test
    ltpRxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable = 0; //unused for inducts (must be set to 0) //must be 0 for this test
    ltpRxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = 0; //unused for inducts (must be set to 0) //must be 0 for this test
    ltpRxCfg.activeSessionDataOnDiskDirectory = "./"; //unused for inducts

    LtpEngineConfig ltpTxCfg;
    ltpTxCfg.thisEngineId = ENGINE_ID_SRC;
    ltpTxCfg.remoteEngineId = ENGINE_ID_DEST; // (not used at the LtpEngine level but at the routing level, so this is "don't care")
    ltpTxCfg.clientServiceId = CLIENT_SERVICE_ID_DEST;
    ltpTxCfg.isInduct = false;
    ltpTxCfg.mtuClientServiceData = 1; //1=> 1 CHARACTER AT A TIME, 
    ltpTxCfg.mtuReportSegment = UINT64_MAX; //unused for outducts //UINT64_MAX=> unlimited report segment size
    ltpTxCfg.oneWayLightTime = ONE_WAY_LIGHT_TIME;
    ltpTxCfg.oneWayMarginTime = ONE_WAY_MARGIN_TIME;
    //ltpTxCfg.remoteHostname = ;
    //ltpTxCfg.remotePort = ;
    //ltpTxCfg.myBoundUdpPort = ;
    //ltpTxCfg.numUdpRxCircularBufferVectors = ;
    ltpTxCfg.estimatedBytesToReceivePerSession = 0; //unused for outducts
    ltpTxCfg.maxRedRxBytesPerSession = 50; //unused for outducts
    ltpTxCfg.checkpointEveryNthDataPacketSender = 0;
    ltpTxCfg.maxRetriesPerSerialNumber = 5;
    ltpTxCfg.force32BitRandomNumbers = false;
    ltpTxCfg.maxSendRateBitsPerSecOrZeroToDisable = 0;
    ltpTxCfg.maxSimultaneousSessions = 100;
    ltpTxCfg.rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable = 0; //unused for outducts
    ltpTxCfg.maxUdpPacketsToSendPerSystemCall = 1; //is a don't care since m_ioServiceLtpEngineThreadPtr is NULL for this test
    ltpTxCfg.senderPingSecondsOrZeroToDisable = 0;
    ltpTxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable = 0; //unused for outducts //must be 0 for this test
    ltpTxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable = 0; //must be 0 for this test
    ltpTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = 0; //must be 0 for this test
    ltpTxCfg.activeSessionDataOnDiskDirectory = "./";

    Test t(ltpRxCfg, ltpTxCfg);
    t.DoTest();
    t.DoTestOneDropSrcToDest();
    t.DoTestTwoDropsSrcToDest();
    t.DoTestTwoDropsConsecutiveMtuContrainedSrcToDest();
    t.DoTestTwoDropsSrcToDestRegularCheckpoints();
    t.DoTestTwoDropsSrcToDestRegularCheckpointsCpBoundary();
    t.DoTestRedAndGreenData();
    t.DoTestFullyGreenData();
    t.DoTestMiscoloredRed();
    t.DoTestMiscoloredGreen();
    t.DoTestTooMuchRedData();
}
