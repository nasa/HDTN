/**
 * @file TestLtpUdpEngine.cpp
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
#include "LtpUdpEngineManager.h"
#include <boost/bind/bind.hpp>
#include "UdpDelaySim.h"
#include <boost/lexical_cast.hpp>
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::unittest;

static const uint64_t ENGINE_ID_SRC(100);
static const uint64_t ENGINE_ID_DEST(200);
static const uint64_t EXPECTED_SESSION_ORIGINATOR_ENGINE_ID(ENGINE_ID_SRC);
static const uint64_t CLIENT_SERVICE_ID_DEST(300);
static const uint64_t DELAY_SENDING_OF_REPORT_SEGMENTS_TIME_MS(20);
static const uint64_t DELAY_SENDING_OF_DATA_SEGMENTS_TIME_MS(20);
static const boost::posix_time::time_duration ACTUAL_DELAY_SRC_TO_DEST(boost::posix_time::milliseconds(10));
static const boost::posix_time::time_duration ACTUAL_DELAY_DEST_TO_SRC(boost::posix_time::milliseconds(10));
static const uint64_t TX_UUID = 123;

BOOST_AUTO_TEST_CASE(LtpUdpEngineTestCase, *boost::unit_test::enabled())
{
    struct MyTransmissionUserData : public LtpTransmissionRequestUserData {
        MyTransmissionUserData() : m_data(0) {}
        MyTransmissionUserData(const unsigned int data) : m_data(data) {}
        virtual ~MyTransmissionUserData() override {}

        unsigned int m_data;
    };

    struct Test {
        std::shared_ptr<LtpUdpEngineManager> ltpUdpEngineManagerSrcPtr;
        std::shared_ptr<LtpUdpEngineManager> ltpUdpEngineManagerDestPtr;
        UdpDelaySim udpDelaySimDataSegmentProxy;
        UdpDelaySim udpDelaySimReportSegmentProxy;
        LtpUdpEngine * ltpUdpEngineSrcPtr;
        LtpUdpEngine * ltpUdpEngineDestPtr;
        const std::string DESIRED_RED_DATA_TO_SEND;
        const std::string DESIRED_RED_AND_GREEN_DATA_TO_SEND;
        const std::string DESIRED_FULLY_GREEN_DATA_TO_SEND;
        boost::condition_variable cv;
        boost::mutex cvMutex;
        

        uint64_t numRedPartReceptionCallbacks;
        uint64_t numSessionStartSenderCallbacks;
        uint64_t numSessionStartReceiverCallbacks;
        uint64_t numGreenPartReceptionCallbacks;
        uint64_t numReceptionSessionCancelledCallbacks;
        uint64_t numTransmissionSessionCompletedCallbacks;
        uint64_t numInitialTransmissionCompletedCallbacks;
        uint64_t numTransmissionSessionCancelledCallbacks;
        uint64_t numOnFailedBundleVecSendCallbacks;
        uint64_t numOnSuccessfulBundleSendCallbacks;

        volatile bool removeCallbackCalled;

        CANCEL_SEGMENT_REASON_CODES lastReasonCode_receptionSessionCancelledCallback;
        CANCEL_SEGMENT_REASON_CODES lastReasonCode_transmissionSessionCancelledCallback;
        Ltp::session_id_t lastSessionId_sessionStartSenderCallback;

        Test(const LtpEngineConfig& ltpRxCfg, const LtpEngineConfig& ltpTxCfg) :
            ltpUdpEngineManagerSrcPtr(LtpUdpEngineManager::GetOrCreateInstance(ltpTxCfg.myBoundUdpPort, true)),
            ltpUdpEngineManagerDestPtr(LtpUdpEngineManager::GetOrCreateInstance(ltpRxCfg.myBoundUdpPort, true)),
            udpDelaySimDataSegmentProxy(ltpTxCfg.remotePort, "localhost", boost::lexical_cast<std::string>(ltpRxCfg.myBoundUdpPort), 1000, 100, ACTUAL_DELAY_SRC_TO_DEST, 0, 0, true),
            udpDelaySimReportSegmentProxy(ltpRxCfg.remotePort, "localhost", boost::lexical_cast<std::string>(ltpTxCfg.myBoundUdpPort), 1000, 100, ACTUAL_DELAY_DEST_TO_SRC, 0, 0, true),
            DESIRED_RED_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!"),
            DESIRED_RED_AND_GREEN_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!GGE"), //G=>green data not EOB, E=>green datat EOB
            DESIRED_FULLY_GREEN_DATA_TO_SEND("GGGGGGGGGGGGGGGGGE"),
            numRedPartReceptionCallbacks(0),
            numSessionStartSenderCallbacks(0),
            numSessionStartReceiverCallbacks(0),
            numGreenPartReceptionCallbacks(0),
            numReceptionSessionCancelledCallbacks(0),
            numTransmissionSessionCompletedCallbacks(0),
            numInitialTransmissionCompletedCallbacks(0),
            numTransmissionSessionCancelledCallbacks(0),
            numOnFailedBundleVecSendCallbacks(0),
            numOnSuccessfulBundleSendCallbacks(0),
            removeCallbackCalled(false),
            lastReasonCode_receptionSessionCancelledCallback(CANCEL_SEGMENT_REASON_CODES::RESERVED),
            lastReasonCode_transmissionSessionCancelledCallback(CANCEL_SEGMENT_REASON_CODES::RESERVED),
            lastSessionId_sessionStartSenderCallback(0, 0)
        {
            BOOST_REQUIRE(ltpUdpEngineManagerSrcPtr->StartIfNotAlreadyRunning()); //already running from constructor so should do nothing and return true 
            BOOST_REQUIRE(ltpUdpEngineManagerSrcPtr->StartIfNotAlreadyRunning()); //still already running so should do nothing and return true 

            ltpUdpEngineDestPtr = ltpUdpEngineManagerDestPtr->GetLtpUdpEnginePtrByRemoteEngineId(EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true); //sessionOriginatorEngineId is the remote engine id in the case of an induct
            if (ltpUdpEngineDestPtr == NULL) {
                ltpUdpEngineManagerDestPtr->AddLtpUdpEngine(ltpRxCfg);
                ltpUdpEngineDestPtr = ltpUdpEngineManagerDestPtr->GetLtpUdpEnginePtrByRemoteEngineId(EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true);
            }
            ltpUdpEngineDestPtr->SetSessionStartCallback(boost::bind(&Test::SessionStartReceiverCallback, this, boost::placeholders::_1));
            ltpUdpEngineDestPtr->SetRedPartReceptionCallback(boost::bind(&Test::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            ltpUdpEngineDestPtr->SetGreenPartSegmentArrivalCallback(boost::bind(&Test::GreenPartSegmentArrivalCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            ltpUdpEngineDestPtr->SetReceptionSessionCancelledCallback(boost::bind(&Test::ReceptionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));

            ltpUdpEngineSrcPtr = ltpUdpEngineManagerSrcPtr->GetLtpUdpEnginePtrByRemoteEngineId(ENGINE_ID_DEST, false);
            if (ltpUdpEngineSrcPtr == NULL) {
                ltpUdpEngineManagerSrcPtr->AddLtpUdpEngine(ltpTxCfg);
                ltpUdpEngineSrcPtr = ltpUdpEngineManagerSrcPtr->GetLtpUdpEnginePtrByRemoteEngineId(ENGINE_ID_DEST, false);
            }

            ltpUdpEngineSrcPtr->SetSessionStartCallback(boost::bind(&Test::SessionStartSenderCallback, this, boost::placeholders::_1));
            ltpUdpEngineSrcPtr->SetTransmissionSessionCompletedCallback(boost::bind(&Test::TransmissionSessionCompletedCallback, this, boost::placeholders::_1, boost::placeholders::_2));
            ltpUdpEngineSrcPtr->SetInitialTransmissionCompletedCallback(boost::bind(&Test::InitialTransmissionCompletedCallback, this, boost::placeholders::_1, boost::placeholders::_2));
            ltpUdpEngineSrcPtr->SetTransmissionSessionCancelledCallback(boost::bind(&Test::TransmissionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
            ltpUdpEngineSrcPtr->SetUserAssignedUuid(TX_UUID);
            ltpUdpEngineSrcPtr->SetOnFailedBundleVecSendCallback(boost::bind(&Test::OnFailedBundleVecSendCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));
            ltpUdpEngineSrcPtr->SetOnSuccessfulBundleSendCallback(boost::bind(&Test::OnSuccessfulBundleSendCallback, this, boost::placeholders::_1, boost::placeholders::_2));
        }

        void RemoveCallback() {
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                removeCallbackCalled = true;
            }
            cv.notify_one();
        }

        ~Test() {
            try {
                LOG_INFO(subprocess) << "waiting to remove ltp dest (induct) for remote engine id " << EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;
                boost::mutex::scoped_lock cvLock(cvMutex);
                removeCallbackCalled = false;
                //sessionOriginatorEngineId is the remote engine id in the case of an induct
                ltpUdpEngineManagerDestPtr->RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true, boost::bind(&Test::RemoveCallback, this));
                while (!removeCallbackCalled) { //lock mutex (above) before checking condition
                    //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
                    if (!cv.timed_wait(cvLock, boost::posix_time::milliseconds(2000))) {
                        LOG_ERROR(subprocess) << "timed out waiting (for 2 seconds) to remove ltp dest (induct) for remote engine id "
                            << EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;
                        break;
                    }
                }
                BOOST_CHECK(removeCallbackCalled);
                LOG_INFO(subprocess) << "removed ltp dest (induct) for remote engine id " << EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;

                LOG_INFO(subprocess) << "waiting to remove ltp src (outduct) for remote engine id " << ENGINE_ID_DEST;
                removeCallbackCalled = false;
                ltpUdpEngineManagerSrcPtr->RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(ENGINE_ID_DEST, false, boost::bind(&Test::RemoveCallback, this));
                while (!removeCallbackCalled) { //lock mutex (above) before checking condition
                    //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
                    if (!cv.timed_wait(cvLock, boost::posix_time::milliseconds(2000))) {
                        LOG_ERROR(subprocess) << "timed out waiting (for 2 seconds) to remove ltp src (outduct) for remote engine id " << ENGINE_ID_DEST;
                        break;
                    }
                }
                BOOST_CHECK(removeCallbackCalled);
                LOG_INFO(subprocess) << "removed ltp src (outduct) for remote engine id " << ENGINE_ID_DEST;
            }
            catch (const boost::condition_error&) {
                BOOST_FAIL("condition_error in LtpUdpEngineTestCase::~Test");
            }
            catch (const boost::thread_resource_error&) {
                BOOST_FAIL("thread_resource_error in LtpUdpEngineTestCase::~Test");
            }
            catch (const boost::thread_interrupted&) {
                BOOST_FAIL("thread_interrupted in LtpUdpEngineTestCase::~Test");
            }
            catch (const boost::lock_error&) {
                BOOST_FAIL("lock_error in LtpUdpEngineTestCase::~Test");
            }
        }

        void SessionStartSenderCallback(const Ltp::session_id_t & sessionId) {
            ++numSessionStartSenderCallbacks;

            //On receiving this notice the client service may, for example, .. remember the
            //session ID so that the session can be canceled in the future if necessary.
            lastSessionId_sessionStartSenderCallback = sessionId;
        }
        void SessionStartReceiverCallback(const Ltp::session_id_t & sessionId) {
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                ++numSessionStartReceiverCallbacks;
                BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            }
            // do not cv.notify_one(); (numSessionStartReceiverCallbacks is not used as a flag)
        }
        void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, padded_vector_uint8_t & movableClientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
            std::string receivedMessage(movableClientServiceDataVec.data(), movableClientServiceDataVec.data() + movableClientServiceDataVec.size());
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                ++numRedPartReceptionCallbacks;
                BOOST_REQUIRE_EQUAL(receivedMessage, DESIRED_RED_DATA_TO_SEND);
                BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            }
            cv.notify_one();
            //std::cout << "receivedMessage: " << receivedMessage << std::endl;
        }
        void GreenPartSegmentArrivalCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec, uint64_t offsetStartOfBlock, uint64_t clientServiceId, bool isEndOfBlock) {
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                ++numGreenPartReceptionCallbacks;
                BOOST_REQUIRE_EQUAL(movableClientServiceDataVec.size(), 1);
                BOOST_REQUIRE_EQUAL(movableClientServiceDataVec[0], (isEndOfBlock) ? 'E' : 'G');

                BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            }
            cv.notify_one();
        }
        void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                ++numReceptionSessionCancelledCallbacks;
                lastReasonCode_receptionSessionCancelledCallback = reasonCode;
                BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            }
            cv.notify_one();
        }
        void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr) {
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                ++numTransmissionSessionCompletedCallbacks;
                BOOST_REQUIRE_EQUAL(userDataPtr.use_count(), 2); //ltp session sender copy + user copy
                MyTransmissionUserData* myUserData = dynamic_cast<MyTransmissionUserData*>(userDataPtr.get());
                BOOST_REQUIRE(myUserData);
                BOOST_REQUIRE_EQUAL(myUserData->m_data, 123);
                BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            }
            cv.notify_one();
        }
        void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr) {
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                BOOST_REQUIRE_EQUAL(userDataPtr.use_count(), 2); //ltp session sender copy + user copy
                MyTransmissionUserData* myUserData = dynamic_cast<MyTransmissionUserData*>(userDataPtr.get());
                BOOST_REQUIRE(myUserData);
                BOOST_REQUIRE_EQUAL(myUserData->m_data, 123);
                BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
                ++numInitialTransmissionCompletedCallbacks;
            }
            cv.notify_one();
        }
        void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr) {
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                BOOST_REQUIRE_EQUAL(userDataPtr.use_count(), 2); //ltp session sender copy + user copy
                MyTransmissionUserData* myUserData = dynamic_cast<MyTransmissionUserData*>(userDataPtr.get());
                BOOST_REQUIRE(myUserData);
                BOOST_REQUIRE_EQUAL(myUserData->m_data, 123);
                ++numTransmissionSessionCancelledCallbacks;
                lastReasonCode_transmissionSessionCancelledCallback = reasonCode;
                BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            }
            cv.notify_one();
        }

        void OnFailedBundleVecSendCallback(std::vector<uint8_t>& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled) {
            std::string failedMessage((char*)movableBundle.data(), ((char*)movableBundle.data()) + movableBundle.size());
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                ++numOnFailedBundleVecSendCallbacks;
                BOOST_REQUIRE_EQUAL(failedMessage, DESIRED_RED_DATA_TO_SEND);
                BOOST_REQUIRE_EQUAL(outductUuid, TX_UUID);
            }
            cv.notify_one();
            
        }
        void OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid) {
            {
                boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
                ++numOnSuccessfulBundleSendCallbacks;
                BOOST_REQUIRE_EQUAL(outductUuid, TX_UUID);
            }
            cv.notify_one();
        }

        void Reset() {
            ltpUdpEngineSrcPtr->Reset_ThreadSafe_Blocking();
            ltpUdpEngineDestPtr->Reset_ThreadSafe_Blocking();
            ltpUdpEngineSrcPtr->SetDeferDelays_ThreadSafe(0, DELAY_SENDING_OF_DATA_SEGMENTS_TIME_MS);
            ltpUdpEngineDestPtr->SetDeferDelays_ThreadSafe(DELAY_SENDING_OF_REPORT_SEGMENTS_TIME_MS, 0);
            ltpUdpEngineSrcPtr->SetCheckpointEveryNthDataPacketForSenders(0);
            ltpUdpEngineDestPtr->SetCheckpointEveryNthDataPacketForSenders(0);
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(UdpDelaySim::UdpDropSimulatorFunction_t());
            udpDelaySimReportSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(UdpDelaySim::UdpDropSimulatorFunction_t());
            {
                boost::mutex::scoped_lock cvLock(cvMutex);
                numRedPartReceptionCallbacks = 0;
                numSessionStartSenderCallbacks = 0;
                numSessionStartReceiverCallbacks = 0;
                numGreenPartReceptionCallbacks = 0;
                numReceptionSessionCancelledCallbacks = 0;
                numTransmissionSessionCompletedCallbacks = 0;
                numInitialTransmissionCompletedCallbacks = 0;
                numTransmissionSessionCancelledCallbacks = 0;
                numOnFailedBundleVecSendCallbacks = 0;
                numOnSuccessfulBundleSendCallbacks = 0;
                lastReasonCode_receptionSessionCancelledCallback = CANCEL_SEGMENT_REASON_CODES::RESERVED;
                lastReasonCode_transmissionSessionCancelledCallback = CANCEL_SEGMENT_REASON_CODES::RESERVED;
                lastSessionId_sessionStartSenderCallback = 0; //sets all fields to 0
            }
        }
        void AssertNoActiveSendersAndReceivers() {
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            {
                BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->NumActiveSenders(), 0);
                BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->NumActiveReceivers(), 0);
                BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->NumActiveSenders(), 0);
                BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->NumActiveReceivers(), 0);
            }
        }
        bool HasActiveSendersAndReceivers() {
            return ltpUdpEngineSrcPtr->NumActiveSenders() || ltpUdpEngineSrcPtr->NumActiveReceivers() || ltpUdpEngineDestPtr->NumActiveSenders() || ltpUdpEngineDestPtr->NumActiveReceivers();
        }
        void TryWaitForNoActiveSendersAndReceivers() {
            for(unsigned int i=0; i<50; ++i) { //max wait 10 seconds
                if(!HasActiveSendersAndReceivers()) {
                    break;
                }
                boost::this_thread::sleep(boost::posix_time::milliseconds(200));
            }
            boost::this_thread::sleep(boost::posix_time::milliseconds(200)); //give extra 200ms before any non-thread-safe Reset() is called (Reset() should not be called in production)
        }

        void DoTest() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;

            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef, 0);
        }

        void DoTestRedAndGreenData() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_AND_GREEN_DATA_TO_SEND.data(), DESIRED_RED_AND_GREEN_DATA_TO_SEND.data() + DESIRED_RED_AND_GREEN_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks && (numGreenPartReceptionCallbacks >= 3)) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;

            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 3);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_AND_GREEN_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_AND_GREEN_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);
        }

        void DoTestFullyGreenData() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_FULLY_GREEN_DATA_TO_SEND.data(), DESIRED_FULLY_GREEN_DATA_TO_SEND.data() + DESIRED_FULLY_GREEN_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = 0;
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numTransmissionSessionCompletedCallbacks && (numGreenPartReceptionCallbacks >= DESIRED_FULLY_GREEN_DATA_TO_SEND.size())) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;

            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1); //works in the fully green case
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);
        }

        
        void DoTestOneDropDataSegmentSrcToDest() {
            struct DropOneSimulation {
                int count;
                DropOneSimulation() : count(0) {}
                bool DoSim(const std::vector<uint8_t> & udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA) {
                        if (++count == 10) {
                            //std::cout << "drop\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropOneSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropOneSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 3); //+3 for 2 Report acks and 1 resend
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 3-1 (see above comment)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef, 1); //for one dropped data segment (dropped => wasn't out of order but still delayed the send)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef, 0);
        }


        
        void DoTestTwoDropDataSegmentSrcToDest() {
            struct DropTwoSimulation {
                int count;
                DropTwoSimulation() : count(0) {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA) {
                        ++count;
                        if ((count == 10) || (count == 13)) {
                            //std::cout << "drop\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropTwoSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropTwoSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 4); //+4 for 2 Report acks and 2 resends
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 4-2 (see above comment)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef, 1); //for dropped data segments (dropped => wasn't out of order but still delayed the send)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef, 0);
        }

        void DoTestTwoDropDataSegmentSrcToDestRegularCheckpoints() {
            struct DropTwoSimulation {
                int count;
                DropTwoSimulation() : count(0) {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA) { //dont skip checkpoints
                        ++count;
                        if ((count == 7) || (count == 13)) {
                            //std::cout << "drop\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropTwoSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropTwoSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->SetCheckpointEveryNthDataPacketForSenders(5);
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 11); //11 (see below comment)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 13); //+13 for 11 Report acks (see next line) and 2 resends
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            //primary first LB: 0, UB: 5
            //primary subsequent LB : 5, UB : 10
            //primary subsequent LB : 10, UB : 15
            //secondary LB : 5, UB : 8
            //primary subsequent LB : 15, UB : 20
            //primary subsequent LB : 20, UB : 25
            //secondary LB : 15, UB : 16
            //primary subsequent LB : 25, UB : 30
            //primary subsequent LB : 30, UB : 35
            //primary subsequent LB : 35, UB : 40
            //primary subsequent LB : 40, UB : 44
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size() + 11); //+11 13-2 (see above comment)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 11); // 44/5=8 + (1 eobCp at 44) + 2 retrans report 
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef, 2); //for 2 retrans report (dropped packets were not out of order but still delayed the send)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentsUnableToBeIssuedRef, 0);
        }

        //this test essentially doesn't do anything new the above does.  The skipped checkpoint is settled at the next checkpoint
        //and the transmission is completed before the timer expires, cancelling it
        void DoTestDropOneCheckpointDataSegmentSrcToDest() {
            struct DropSimulation {
                int count;
                DropSimulation() : count(0) {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT) { //skip only non-EORP-EOB checkpoints
                        ++count;
                        if (count == 2) {
                            LOG_INFO(subprocess) << "drop";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->SetCheckpointEveryNthDataPacketForSenders(5);
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 9); //9 (see below comment)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 10); //+10 for 9 Report acks (see next line) and 1 resend
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            //primary first LB: 0, UB: 5
            //primary subsequent LB : 5, UB : 15
            //primary subsequent LB : 15, UB : 20
            //secondary LB : 5, UB : 10
            //primary subsequent LB : 20, UB : 25
            //primary subsequent LB : 25, UB : 30
            //primary subsequent LB : 30, UB : 35
            //primary subsequent LB : 35, UB : 40
            //primary subsequent LB : 40, UB : 44
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size() + 9); //+9 10-1 (see above comment)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 9); // 44/5-1=7 + (1 eobCp at 44) + 1 retrans report 
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numDiscretionaryCheckpointsNotResentRef, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef, 1); //for 1 retrans report (dropped packets were not out of order but still delayed the send)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentsUnableToBeIssuedRef, 0);
        }

        void DoTestDropEOBCheckpointDataSegmentSrcToDest() {
            struct DropSimulation {
                int count;
                DropSimulation() : count(0) {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK)) {
                        ++count;
                        if (count == 1) {
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 1); //1 for 1 Report segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack and 1 resend CP_EOB
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 1 Report ack and 1 resend CP_EOB and -1failedEOB
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); //1 for 1 Report segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacksRef, 1);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef, 0);
        }

        void DoTestDropRaSrcToDest() {
            struct DropSimulation {
                int count;
                DropSimulation() : count(0) {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if ((type == LTP_SEGMENT_TYPE_FLAGS::REPORT_ACK_SEGMENT)) {
                        ++count;
                        if (count == 1) {
                            LOG_INFO(subprocess) << "drop";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                //lock mutex (above) before checking condition (although m_numReportSegmentTimerExpiredCallbacks is not set within the mutex)
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks && (ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacksRef == 1)) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 2); //2 for 1 Report segment + 1 Resend Report Segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack and 1 resend Report Ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 resend Report Ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 2); //2 for 1 Report segment + 1 Resend Report Segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacksRef, 1);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
        }
        /*
        void DoTestDropReportSegmentDestToSrc() {
            struct DropSimulationSecondEob {
                int count;
                DropSimulationSecondEob() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK)) {
                        ++count;
                        if (count == 2) {
                            return true;
                        }
                    }
                    return false;
                }
            };
            struct DropSimulationFirstRs {
                int count;
                DropSimulationFirstRs() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REPORT_SEGMENT) {
                        ++count;
                        if (count == 1) {
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulationSecondEob simEob;
            DropSimulationFirstRs simRs;
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulationSecondEob::DoSim, &simEob, boost::placeholders::_1);
            ltpUdpEngineDestPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulationFirstRs::DoSim, &simRs, boost::placeholders::_1);
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
                //ltpUdpEngineSrcPtr->SignalReadyForSend_ThreadSafe();
                //ltpUdpEngineDestPtr->SignalReadyForSend_ThreadSafe();
            }
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack and 1 resend CP_EOB
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 1); //1 for 1 Report segment

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numTimerExpiredCallbacks, 1);
        }
        */


        //src checkpoint should expire until limit then send cancel segment to receiver
        void DoTestDropEOBAlwaysCheckpointDataSegmentSrcToDest() {
            struct DropSimulation {
                DropSimulation() {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    return ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK));
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 100; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numReceptionSessionCancelledCallbacks && numTransmissionSessionCancelledCallbacks && numOnFailedBundleVecSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(500));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 1); // 1 for cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 6); //+6 for 5 resend EOB and 1 cancel segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size()); //+0 for -1EOB +1  cancel segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); // 1 for cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacksRef, 6);
        }

        //dest RS timer should expire until limit then send cancel segment to sender
        void DoTestDropRaAlwaysSrcToDest() {
            struct DropSimulation {
                DropSimulation() {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    return ((type == LTP_SEGMENT_TYPE_FLAGS::REPORT_ACK_SEGMENT));
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 100; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numReceptionSessionCancelledCallbacks && numTransmissionSessionCompletedCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(500));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 7); //7 see comment below
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 7); //+7 for 6 RA and 1 CA
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for 1 CA (6 RA always dropped)
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 7); //7 for 1 RS, 5 resend RS, and 1 cancel segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1); //session won't be found when CS arrives at sender
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0); //session won't be found when CS arrives at sender
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacksRef, 6);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
        }

        //src checkpoint should never make it to receiver, giving receiver time to cancel session
        void DoTestReceiverCancelSession() {
            struct DropSimulation {
                DropSimulation() {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    return ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK));
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                //lock mutex (above) before checking condition (although m_countUdpPacketsReceived not in mutex)
                if (numInitialTransmissionCompletedCallbacks && (ltpUdpEngineDestPtr->m_countUdpPacketsReceived == DESIRED_RED_DATA_TO_SEND.size() - 1)) { 
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(250));
            }
            ltpUdpEngineDestPtr->CancellationRequest_ThreadSafe(lastSessionId_sessionStartSenderCallback); //cancel from receiver
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 1); //1 cancel segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, (DESIRED_RED_DATA_TO_SEND.size() - 1) + 1); //+1 cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); //1 cancel segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 1); //failed because receiver cancelled session
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            //BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
        }

        //src checkpoint should never make it to receiver, giving receiver time to cancel session
        void DoTestReceiverCancelSessionInvalidClientServiceId() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST + 1; //+1 to make it invalid
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                //lock mutex (above) before checking condition (although m_countUdpPacketsReceived not in mutex)
                if (numReceptionSessionCancelledCallbacks && numTransmissionSessionCancelledCallbacks && numOnFailedBundleVecSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(250));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 1); // 1 for cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); //1 cancel segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 1); //failed because receiver cancelled session
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::UNREACHABLE);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            //BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::UNREACHABLE);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
        }

        //src checkpoint should never make it to receiver, giving sender time to cancel session
        void DoTestSenderCancelSession() {
            struct DropSimulation {
                DropSimulation() {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    return ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK));
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numInitialTransmissionCompletedCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(250));
            }
            ltpUdpEngineSrcPtr->CancellationRequest_ThreadSafe(lastSessionId_sessionStartSenderCallback); //cancel from sender
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 1); //1 cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 cancel req
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, (DESIRED_RED_DATA_TO_SEND.size() - 1) + 1); //+1 cancel req
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); //1 cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 1); //failed because sender cancelled session (and gets the data back)
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
            //BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
        }

        void DoTestDropOddDataSegmentWithRsMtu() {
            struct DropSimulation {
                int count;
                DropSimulation() : count(0) {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA) {
                        ++count;
                        if ((count < 30) && (count & 1)) {
                            //std::cout << "drop odd\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            //expect:
            //  max reception claims = 3
            //  drop odd (printed 15x)                
            //  splitting 1 report segment with 15 reception claims into 5 report segments with no more than 3 reception claims per report segment
            Reset();
            ltpUdpEngineDestPtr->SetMtuReportSegment_ThreadSafe(110); // 110 bytes will result in 3 reception claims max
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 10); //10 see comment below
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 25); //+25 for 10 Report acks (see below) and 15 resends
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_RED_DATA_TO_SEND.size() + 25 - 15); //-15 for 15 resends
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 10); //10 for 5 initial separately sentReport segments + 5 report segments of data complete as response to resends
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            ltpUdpEngineDestPtr->SetMtuReportSegment_ThreadSafe(UINT64_MAX); //restore to default unlimited reception claims
        }
        
        //test receiver stagnant session timeout
        void DoTestDropGreenEobSrcToDest() {
            struct DropOneSimulation {
                int count;
                DropOneSimulation() : count(0) {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::GREENDATA_ENDOFBLOCK) {
                        //std::cout << "drop green eob\n";
                        return true;
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropOneSimulation sim;
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropOneSimulation::DoSim, &sim, boost::placeholders::_1));
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_FULLY_GREEN_DATA_TO_SEND.data(), DESIRED_FULLY_GREEN_DATA_TO_SEND.data() + DESIRED_FULLY_GREEN_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = 0;
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 60; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                //lock mutex (above) before checking condition
                if (numTransmissionSessionCompletedCallbacks && numReceptionSessionCancelledCallbacks && numOnSuccessfulBundleSendCallbacks && (numGreenPartReceptionCallbacks >= (DESIRED_FULLY_GREEN_DATA_TO_SEND.size() - 1))) {
                    break;
                }
                //std::cout << "delay" << i << "\n";
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1); //is set for fully green case
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, DESIRED_FULLY_GREEN_DATA_TO_SEND.size() - 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, 1); //1 cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_FULLY_GREEN_DATA_TO_SEND.size() + 1); //+1 for cancel ack, not //+1 for 1 (last) dropped packet
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);
            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, DESIRED_FULLY_GREEN_DATA_TO_SEND.size()); //+0 for -1 dropped green eob  +1 cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); //1 for housekeeping sending CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED 
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);
        }

        void DoTestReverseStartToCpSrcToDest(bool addTwoDiscretionaryCheckpoints, bool reverseOnlyFromEob, bool disableRsDefer) {
            struct DropSimulation {
                typedef std::pair<std::vector<uint8_t>, std::size_t> udppacket_plus_size_pair_t;
                std::list<udppacket_plus_size_pair_t> m_packetsToReverse;
                UdpDelaySim & m_udpDelaySimDataSegmentProxyRef;
                bool m_reverseOnlyFromEob;
                DropSimulation(UdpDelaySim & udpDelaySimDataSegmentProxy, bool paramReverseOnlyFromEob) :
                    m_udpDelaySimDataSegmentProxyRef(udpDelaySimDataSegmentProxy),
                    m_reverseOnlyFromEob(paramReverseOnlyFromEob) {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived, std::size_t bytesTransferred) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const bool isRedData = (ltpHeaderByte <= 3);
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (isRedData) {
                        std::vector<uint8_t> packetCopy(udpPacketReceived);
                        m_packetsToReverse.emplace_front(std::move(packetCopy), bytesTransferred);

                        const bool isRedCheckpoint = (ltpHeaderByte != 0);
                        const bool isEndOfRedPart = (ltpHeaderByte & 2);
                        if ((isRedCheckpoint && (!m_reverseOnlyFromEob)) || (isEndOfRedPart && m_reverseOnlyFromEob)) {
                            //send packets out in reverse order
                            for (std::list<udppacket_plus_size_pair_t>::iterator it = m_packetsToReverse.begin(); it != m_packetsToReverse.end(); ++it) {
                                m_udpDelaySimDataSegmentProxyRef.QueuePacketForDelayedSend_NotThreadSafe(it->first, it->second); //this is safe since callback is called by proxy thread
                            }
                            m_packetsToReverse.clear();
                        }
                        return true; //drop within udp receive callback for now (was already buffered above so not really dropped)
                    }
                    return false;
                }
            };
#if 0
            struct DropSimulationReportSegments {
                typedef std::pair<std::vector<uint8_t>, std::size_t> udppacket_plus_size_pair_t;
                std::list<udppacket_plus_size_pair_t> m_packetsToReverse;
                UdpDelaySim& m_udpDelaySimReportSegmentProxyRef;
                const uint64_t m_numReportsToReverse;
                DropSimulationReportSegments(UdpDelaySim& udpDelaySimReportSegmentProxy, uint64_t numReportsToReverse) :
                    m_udpDelaySimReportSegmentProxyRef(udpDelaySimReportSegmentProxy),
                    m_numReportsToReverse(numReportsToReverse) {}
                bool DoSim(const std::vector<uint8_t>& udpPacketReceived, std::size_t bytesTransferred) {
                    const uint8_t ltpHeaderByte = udpPacketReceived[0];
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REPORT_SEGMENT) {
                        
                        std::vector<uint8_t> packetCopy(udpPacketReceived);
                        m_packetsToReverse.emplace_front(std::move(packetCopy), bytesTransferred);
                        std::cout << "buf " << m_packetsToReverse.size() << " " << m_numReportsToReverse << "\n";
                        if (m_packetsToReverse.size() == m_numReportsToReverse) {
                            std::cout << "rev\n";
                            //send packets out in reverse order
                            for (std::list<udppacket_plus_size_pair_t>::iterator it = m_packetsToReverse.begin(); it != m_packetsToReverse.end(); ++it) {
                                m_udpDelaySimReportSegmentProxyRef.QueuePacketForDelayedSend_NotThreadSafe(it->first, it->second); //this is safe since callback is called by proxy thread
                            }
                            m_packetsToReverse.clear();
                        }
                        return true; //drop within udp receive callback for now (was already buffered above so not really dropped)
                    }
                    return false;
                }
            };
#endif
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim(udpDelaySimDataSegmentProxy, reverseOnlyFromEob);
            udpDelaySimDataSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1, boost::placeholders::_2));
            //DropSimulationReportSegments rsSim(udpDelaySimReportSegmentProxy, addTwoDiscretionaryCheckpoints ? 4 : 1);
            if (disableRsDefer) {
                //udpDelaySimReportSegmentProxy.SetUdpDropSimulatorFunction_ThreadSafe(boost::bind(&DropSimulationReportSegments::DoSim, &rsSim, boost::placeholders::_1, boost::placeholders::_2));
                ltpUdpEngineDestPtr->SetDeferDelays_ThreadSafe(0, 0);
            }
            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            if (addTwoDiscretionaryCheckpoints) {
                ltpUdpEngineSrcPtr->SetCheckpointEveryNthDataPacketForSenders(18);
            }
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                boost::mutex::scoped_lock cvLock(cvMutex);
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && numOnSuccessfulBundleSendCallbacks) { //lock mutex (above) before checking condition
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            boost::mutex::scoped_lock cvLock(cvMutex); //boost unit test assertions are not thread safe
            if (disableRsDefer) {
                if (addTwoDiscretionaryCheckpoints) {
                    if (reverseOnlyFromEob) {
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack + 1 async report ack
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 2); //2 for 1 gapped Report segment + 1 async report
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, 0); //feature disabled by disableRsDefer
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 0); //feature disabled by disableRsDefer
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentsUnableToBeIssuedRef, 2);
                        //since rs defer disabled on receiver, 1 gapped Report segments ended up being filled on sender (not requiring any data segments to be sent)
                        //the async reception report had same bounds as other report segment and not counted below
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numDeletedFullyClaimedPendingReportsRef, 1);
                    }
                    else {
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 4); //+4 for 3 Report acks plus 1 ack from async rs
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 4); //4 for 3 Report segments plus 1 async rs
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, 0); //feature disabled by disableRsDefer
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 0); //feature disabled by disableRsDefer
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentsUnableToBeIssuedRef, 0);
                        //since rs defer disabled on receiver, 3 gapped Report segments ended up being filled on sender (not requiring any data segments to be sent)
                        //the async reception report did not have the same bounds as any other report segment and counted below
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numDeletedFullyClaimedPendingReportsRef, 3+1);
                    }
                }
                else {
                    
                    BOOST_REQUIRE(!reverseOnlyFromEob);
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack + 1 async report ack
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 2); //2 for 1 gapped Report segment + 1 async report
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, 0); //feature disabled by disableRsDefer
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 0); //feature disabled by disableRsDefer
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentsUnableToBeIssuedRef, 0);
                    //since rs defer disabled on receiver, 1 gapped Report segments ended up being filled on sender (not requiring any data segments to be sent)
                    //the async reception report had same bounds as other report segment and not counted below
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numDeletedFullyClaimedPendingReportsRef, 1);
                }
            }
            else {
                if (addTwoDiscretionaryCheckpoints) {
                    //Related to Github issue #22 Defer synchronous reception report with out-of-order data segments:
                    //...In a situation with no loss but lots of out-of-order delivery this will have exactly the same number of reports,
                    //they will just be sent when the full checkpointed bounds of data have been received.
                    if (reverseOnlyFromEob) {
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for 1 Report ack
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); //1 for 1 Report segment
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, DESIRED_RED_DATA_TO_SEND.size() - 1); //-1 to exclude only checkpoint
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 1);
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentsUnableToBeIssuedRef, 2);
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numDeletedFullyClaimedPendingReportsRef, 0); //despite ds defer on sender enabled, not needed since rsDefer on receiver preventing the need
                    }
                    else {
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 4); //+4 for 3 Report acks plus 1 ack from async rs
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 4); //4 for 3 Report segments plus 1 async rs
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, DESIRED_RED_DATA_TO_SEND.size() - 3); //-3 to exclude 3 checkpoints
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 3);
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentsUnableToBeIssuedRef, 0);
                        BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numDeletedFullyClaimedPendingReportsRef, 0); //despite ds defer on sender enabled, not needed since rsDefer on receiver preventing the need
                    }
                }
                else {
                    BOOST_REQUIRE(!reverseOnlyFromEob);
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for 1 Report ack
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent, 1); //1 for 1 Report segment
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numGapsFilledByOutOfOrderDataSegmentsRef, DESIRED_RED_DATA_TO_SEND.size() - 1); //-1 to exclude only checkpoint
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef, 1);
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentsUnableToBeIssuedRef, 0);
                    BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numDeletedFullyClaimedPendingReportsRef, 0); //despite ds defer on sender enabled, not needed since rsDefer on receiver preventing the need
                }
            }

            //no dropped packets
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countUdpPacketsReceived, ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countUdpPacketsReceived, ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent);
            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls, ltpUdpEngineSrcPtr->m_countBatchSendCalls);

            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls, ltpUdpEngineDestPtr->m_countBatchSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnSuccessfulBundleSendCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numOnFailedBundleVecSendCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacksRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacksRef, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef, 0);
        }

    };

    
    const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME(boost::posix_time::milliseconds(250));
    const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(250));
    
    
    
    
    const uint16_t BOUND_UDP_PORT_SRC(12345);
    const uint16_t BOUND_UDP_PORT_DEST(1113);
    const uint16_t BOUND_UDP_PORT_DATA_SEGMENT_PROXY(12346);
    const uint16_t BOUND_UDP_PORT_REPORT_SEGMENT_PROXY(12347);

    

    LtpEngineConfig ltpRxCfg;

    ltpRxCfg.thisEngineId = ENGINE_ID_DEST;
    ltpRxCfg.remoteEngineId = EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;// (i.e. ENGINE_ID_SRC); //expectedSessionOriginatorEngineId to be received
    ltpRxCfg.clientServiceId = CLIENT_SERVICE_ID_DEST; //not currently checked by induct
    ltpRxCfg.isInduct = true;
    ltpRxCfg.mtuClientServiceData = 1; //unused for inducts
    ltpRxCfg.mtuReportSegment = UINT64_MAX; //UINT64_MAX=> unlimited report segment size
    ltpRxCfg.oneWayLightTime = ONE_WAY_LIGHT_TIME;
    ltpRxCfg.oneWayMarginTime = ONE_WAY_MARGIN_TIME;
    ltpRxCfg.remoteHostname = "localhost";
    ltpRxCfg.remotePort = BOUND_UDP_PORT_REPORT_SEGMENT_PROXY;
    ltpRxCfg.myBoundUdpPort = BOUND_UDP_PORT_DEST;
    ltpRxCfg.numUdpRxCircularBufferVectors = 100;
    ltpRxCfg.estimatedBytesToReceivePerSession = 0; //force a resize
    ltpRxCfg.maxRedRxBytesPerSession = 10000000;
    ltpRxCfg.checkpointEveryNthDataPacketSender = 0; //unused for inducts
    ltpRxCfg.maxRetriesPerSerialNumber = 5;
    ltpRxCfg.force32BitRandomNumbers = false;
    ltpRxCfg.maxSendRateBitsPerSecOrZeroToDisable = 0;
    ltpRxCfg.maxSimultaneousSessions = 8;
    ltpRxCfg.rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable = 1000;
    ltpRxCfg.maxUdpPacketsToSendPerSystemCall = 1;
    ltpRxCfg.senderPingSecondsOrZeroToDisable = 0; //unused for inducts
    ltpRxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable = DELAY_SENDING_OF_REPORT_SEGMENTS_TIME_MS; 
    ltpRxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable = 0; //unused for inducts (must be set to 0) 
    ltpRxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = 0;
    ltpRxCfg.activeSessionDataOnDiskDirectory = "./";

    LtpEngineConfig ltpTxCfg;
    ltpTxCfg.thisEngineId = ENGINE_ID_SRC;
    ltpTxCfg.remoteEngineId = ENGINE_ID_DEST;
    ltpTxCfg.clientServiceId = CLIENT_SERVICE_ID_DEST;
    ltpTxCfg.isInduct = false;
    ltpTxCfg.mtuClientServiceData = 1; //1=> 1 CHARACTER AT A TIME, 
    ltpTxCfg.mtuReportSegment = UINT64_MAX; //unused for outducts //UINT64_MAX=> unlimited report segment size
    ltpTxCfg.oneWayLightTime = ONE_WAY_LIGHT_TIME;
    ltpTxCfg.oneWayMarginTime = ONE_WAY_MARGIN_TIME;
    ltpTxCfg.remoteHostname = "localhost";
    ltpTxCfg.remotePort = BOUND_UDP_PORT_DATA_SEGMENT_PROXY;
    ltpTxCfg.myBoundUdpPort = BOUND_UDP_PORT_SRC;
    ltpTxCfg.numUdpRxCircularBufferVectors = 100;
    ltpTxCfg.estimatedBytesToReceivePerSession = 0; //unused for outducts
    ltpTxCfg.maxRedRxBytesPerSession = 0; //unused for outducts
    ltpTxCfg.checkpointEveryNthDataPacketSender = 0;
    ltpTxCfg.maxRetriesPerSerialNumber = 5;
    ltpTxCfg.force32BitRandomNumbers = false;
    ltpTxCfg.maxSendRateBitsPerSecOrZeroToDisable = 0;
    ltpTxCfg.maxSimultaneousSessions = 8;
    ltpTxCfg.rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable = 0; //unused for outducts
    ltpTxCfg.maxUdpPacketsToSendPerSystemCall = 1;
    ltpTxCfg.senderPingSecondsOrZeroToDisable = 0;
    ltpTxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable = 0; //unused for outducts 
    ltpTxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable = DELAY_SENDING_OF_DATA_SEGMENTS_TIME_MS;
    ltpTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = 0;
    ltpTxCfg.activeSessionDataOnDiskDirectory = "./";

    //TEST WITH 1 maxUdpPacketsToSendPerSystemCall (NO BATCH SEND)
    LOG_INFO(subprocess) << "+++START 1 PACKET PER SYSTEM CALL+++";
    {
        LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(UINT16_MAX); //MUST BE CALLED BEFORE Test Constructor
        ltpRxCfg.maxUdpPacketsToSendPerSystemCall = 1;
        ltpTxCfg.maxUdpPacketsToSendPerSystemCall = 1;
        Test t(ltpRxCfg, ltpTxCfg);

        //disable delayed report segments (3rd parameter)
        t.DoTestReverseStartToCpSrcToDest(false, false, true); //the only cp is EOB
        t.DoTestReverseStartToCpSrcToDest(true, false, true); //two discretionary cp, reverse from 1st cp
        t.DoTestReverseStartToCpSrcToDest(true, true, true); //two discretionary cp, reverse from EOB so those reports cannot be issued

        //enable delayed report segments (3rd parameter)
        t.DoTestReverseStartToCpSrcToDest(false, false, false); //the only cp is EOB
        t.DoTestReverseStartToCpSrcToDest(true, false, false); //two discretionary cp, reverse from 1st cp
        t.DoTestReverseStartToCpSrcToDest(true, true, false); //two discretionary cp, reverse from EOB so those reports cannot be issued
        
        t.DoTest();
        t.DoTestRedAndGreenData();
        t.DoTestFullyGreenData();
        LOG_INFO(subprocess) << "-----START LONG TEST (STAGNANT GREEN LTP DROPS EOB)---------";
        t.DoTestDropGreenEobSrcToDest();
        LOG_INFO(subprocess) << "-----END LONG TEST (STAGNANT GREEN LTP DROPS EOB)---------";
        t.DoTestOneDropDataSegmentSrcToDest();
        t.DoTestTwoDropDataSegmentSrcToDest();
        t.DoTestTwoDropDataSegmentSrcToDestRegularCheckpoints();
        t.DoTestDropOneCheckpointDataSegmentSrcToDest();
        t.DoTestDropEOBCheckpointDataSegmentSrcToDest();
        t.DoTestDropRaSrcToDest();
        LOG_INFO(subprocess) << "-----START LONG TEST (RED LTP ALWAYS DROPS EOB)---------";
        t.DoTestDropEOBAlwaysCheckpointDataSegmentSrcToDest();
        LOG_INFO(subprocess) << "-----END LONG TEST (RED LTP ALWAYS DROPS EOB)---------";
        t.DoTestDropRaAlwaysSrcToDest();
        t.DoTestReceiverCancelSession();
        t.DoTestReceiverCancelSessionInvalidClientServiceId();
        t.DoTestSenderCancelSession();
        t.DoTestDropOddDataSegmentWithRsMtu();
    }
    LOG_INFO(subprocess) << "+++END 1 PACKET PER SYSTEM CALL+++";
    LOG_INFO(subprocess) << "+++START 500 PACKETS PER SYSTEM CALL+++";
    {
        LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(UINT16_MAX); //MUST BE CALLED BEFORE Test Constructor
        ltpRxCfg.maxUdpPacketsToSendPerSystemCall = 500;
        ltpTxCfg.maxUdpPacketsToSendPerSystemCall = 500;
        Test t(ltpRxCfg, ltpTxCfg);
        t.DoTest();
        t.DoTestRedAndGreenData();
        t.DoTestFullyGreenData();
    }
    LOG_INFO(subprocess) << "+++END 500 PACKETS PER SYSTEM CALL+++";
    LOG_INFO(subprocess) << "+++START SESSION ON DISK AND 1 PACKET PER SYSTEM CALL+++";
    {
        LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(UINT16_MAX); //MUST BE CALLED BEFORE Test Constructor
        ltpRxCfg.maxUdpPacketsToSendPerSystemCall = 1;
        ltpTxCfg.maxUdpPacketsToSendPerSystemCall = 1;
        ltpRxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = 2000;
        ltpTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = 2000;
        Test t(ltpRxCfg, ltpTxCfg);
        t.DoTest();
        t.DoTestRedAndGreenData();
        t.DoTestFullyGreenData();
    }
    LOG_INFO(subprocess) << "+++END SESSION ON DISK AND 1 PACKET PER SYSTEM CALL+++";
    LOG_INFO(subprocess) << "+++START SESSION ON DISK AND 500 PACKETS PER SYSTEM CALL+++";
    {
        LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(UINT16_MAX); //MUST BE CALLED BEFORE Test Constructor
        ltpRxCfg.maxUdpPacketsToSendPerSystemCall = 500;
        ltpTxCfg.maxUdpPacketsToSendPerSystemCall = 500;
        ltpRxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = 2000;
        ltpTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = 2000;
        Test t(ltpRxCfg, ltpTxCfg);
        t.DoTest();
        t.DoTestRedAndGreenData();
        t.DoTestFullyGreenData();
    }
    LOG_INFO(subprocess) << "+++END SESSION ON DISK AND 500 PACKETS PER SYSTEM CALL+++";
}
