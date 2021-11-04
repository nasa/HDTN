#include <boost/test/unit_test.hpp>
#include "TcpclV4.h"
#include <boost/bind/bind.hpp>



BOOST_AUTO_TEST_CASE(TcpclV4FullTestCase)
{
    struct Test {
        TcpclV4 m_tcpcl;
        bool m_useTls;
        uint16_t m_keepAliveInterval;
        uint64_t m_segmentMru;
        uint64_t m_transferMru;
        const std::string m_remoteNodeEidUri;
        TcpclV4::tcpclv4_extensions_t m_sessionExtensions;
        bool m_ackIsStart;
        bool m_ackIsEnd;
        uint64_t m_ackTransferId;
        uint64_t m_ackBytesAcknowledged;
        uint64_t m_bundleRefusalTransferId;
        TCPCLV4_SESSION_TERMINATION_REASON_CODES m_sessionTerminationReasonCode;
        bool m_isAckOfAnEarlierSessionTerminationMessage;
        const std::string m_bundleDataToSendNoFragment;
        unsigned int m_numContactHeaderCallbackCount;
        unsigned int m_numSessionInitCallbackCount;
        uint64_t m_numSessionExtensionsProcessed;
        unsigned int m_numDataSegmentCallbackCountNoFragment;
        unsigned int m_numDataSegmentCallbackCountWithFragments;
        unsigned int m_numAckCallbackCount;
        unsigned int m_numBundleRefusalCallbackCount;
        unsigned int m_numMessageRejectCallbackCount;
        unsigned int m_numKeepAliveCallbackCount;
        unsigned int m_numSessionTerminationMessageCallbackCount;
        std::string m_fragmentedBundleRxConcat;
        Test() :
            m_useTls(false),
            m_keepAliveInterval(0x1234),
            m_segmentMru(1000000),
            m_transferMru(2000000),
            m_remoteNodeEidUri("test Eid String!"),
            m_ackIsStart(true),
            m_ackIsEnd(true),
            m_ackTransferId(12345678910),
            m_ackBytesAcknowledged(234567891011),
            m_bundleRefusalTransferId(111111111111),
            m_sessionTerminationReasonCode(TCPCLV4_SESSION_TERMINATION_REASON_CODES::RESOURCE_EXHAUSTION),
            m_isAckOfAnEarlierSessionTerminationMessage(true),
            m_bundleDataToSendNoFragment("this is a test bundle"),
            m_numContactHeaderCallbackCount(0),
            m_numSessionInitCallbackCount(0),
            m_numSessionExtensionsProcessed(0),
            m_numDataSegmentCallbackCountNoFragment(0),
            m_numDataSegmentCallbackCountWithFragments(0),
            m_numAckCallbackCount(0),
            m_numBundleRefusalCallbackCount(0),
            m_numMessageRejectCallbackCount(0),
            m_numKeepAliveCallbackCount(0),
            m_numSessionTerminationMessageCallbackCount(0),
            m_fragmentedBundleRxConcat("")
        {

        }

        void DoRxContactHeader() {
            m_tcpcl.SetContactHeaderReadCallback(boost::bind(&Test::ContactHeaderCallback, this, boost::placeholders::_1));

            std::vector<uint8_t> hdr;
            TcpclV4::GenerateContactHeader(hdr, m_useTls);
            m_tcpcl.HandleReceivedChars(hdr.data(), hdr.size());
        }

        void DoSessionInit()
        {
            m_tcpcl.SetSessionInitReadCallback(boost::bind(&Test::SessionInitReadCallback, this, boost::placeholders::_1,
                boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4, boost::placeholders::_5));

            std::vector<uint8_t> msg;
            TcpclV4::GenerateSessionInitMessage(msg, m_keepAliveInterval, m_segmentMru, m_transferMru, m_remoteNodeEidUri, m_sessionExtensions);
            m_tcpcl.HandleReceivedChars(msg.data(), msg.size());
        }
        
        void DoAck() {
            m_tcpcl.SetAckSegmentReadCallback(boost::bind(&Test::AckCallback, this, boost::placeholders::_1,
                boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));

            std::vector<uint8_t> ackSegment;
            TcpclV4::GenerateAckSegment(ackSegment, m_ackIsStart, m_ackIsEnd, m_ackTransferId, m_ackBytesAcknowledged);
            m_tcpcl.HandleReceivedChars(ackSegment.data(), ackSegment.size());
        }

        void DoBundleRefusal() {
            m_tcpcl.SetBundleRefusalCallback(boost::bind(&Test::BundleRefusalCallback, this, boost::placeholders::_1, boost::placeholders::_2));

            std::vector<uint8_t> bundleRefusalSegment;
            TcpclV4::GenerateBundleRefusal(bundleRefusalSegment, TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_EXTENSION_FAILURE, m_bundleRefusalTransferId);
            m_tcpcl.HandleReceivedChars(bundleRefusalSegment.data(), bundleRefusalSegment.size());
        }

        void DoMessageReject() {
            m_tcpcl.SetMessageRejectCallback(boost::bind(&Test::MessageRejectCallback, this, boost::placeholders::_1, boost::placeholders::_2));

            std::vector<uint8_t> messageRejectSegment;
            TcpclV4::GenerateMessageRejection(messageRejectSegment, TCPCLV4_MESSAGE_REJECT_REASON_CODES::MESSAGE_TYPE_UNKNOWN, 253);
            m_tcpcl.HandleReceivedChars(messageRejectSegment.data(), messageRejectSegment.size());
        }

        

        void DoKeepAlive() {
            m_tcpcl.SetKeepAliveCallback(boost::bind(&Test::KeepAliveCallback, this));

            std::vector<uint8_t> keepAliveSegment;
            TcpclV4::GenerateKeepAliveMessage(keepAliveSegment);
            m_tcpcl.HandleReceivedChars(keepAliveSegment.data(), keepAliveSegment.size());
        }

        void DoSessionTermination() {
            m_tcpcl.SetSessionTerminationMessageCallback(boost::bind(&Test::SessionTerminationMessageCallback, this, boost::placeholders::_1, boost::placeholders::_2));

            std::vector<uint8_t> sessionTerminationMessage;
            TcpclV4::GenerateSessionTerminationMessage(sessionTerminationMessage, m_sessionTerminationReasonCode, m_isAckOfAnEarlierSessionTerminationMessage);

            m_tcpcl.HandleReceivedChars(sessionTerminationMessage.data(), sessionTerminationMessage.size());
        }
/*
        

        void DoDataSegmentNoFragment() {
            std::vector<uint8_t> bundleSegment;
            m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&Test::DataSegmentCallbackNoFragment, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
            Tcpcl::GenerateDataSegment(bundleSegment, true, true, (const uint8_t*)m_bundleDataToSendNoFragment.data(), m_bundleDataToSendNoFragment.size());
            m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());
        }

        void DoDataSegmentNoFragmentCharByChar() { //skip sdnv shortcut in data segment
            std::vector<uint8_t> bundleSegment;
            m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&Test::DataSegmentCallbackNoFragment, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
            Tcpcl::GenerateDataSegment(bundleSegment, true, true, (const uint8_t*)m_bundleDataToSendNoFragment.data(), m_bundleDataToSendNoFragment.size());
            for (std::size_t i = 0; i < bundleSegment.size(); ++i) {
                m_tcpcl.HandleReceivedChar(bundleSegment[i]);
            }
        }

        void DoDataSegmentThreeFragments() {
            m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&Test::DataSegmentCallbackWithFragments, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
            std::vector<uint8_t> bundleSegment;

            BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
            BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, std::string(""));
            BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 0);
            static const std::string f1 = "fragOne ";
            Tcpcl::GenerateDataSegment(bundleSegment, true, false, (const uint8_t*)f1.data(), f1.size());
            m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());

            BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
            BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, f1);
            BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 1);
            static const std::string f2 = "fragTwo ";
            Tcpcl::GenerateDataSegment(bundleSegment, false, false, (const uint8_t*)f2.data(), f2.size());
            m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());

            BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
            BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, f1 + f2);
            BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 2);
            static const std::string f3 = "fragThree";
            Tcpcl::GenerateDataSegment(bundleSegment, false, true, (const uint8_t*)f3.data(), f3.size());
            m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());

            BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
            BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, f1+f2+f3);
            BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 3);
        }
        */
        void ContactHeaderCallback(bool remoteHasEnabledTlsSecurity) {
            ++m_numContactHeaderCallbackCount;
            BOOST_REQUIRE(m_useTls == remoteHasEnabledTlsSecurity);
        }
        void SessionInitReadCallback(uint16_t keepAliveIntervalSeconds, uint64_t segmentMru, uint64_t transferMru,
            const std::string & remoteNodeEidUri, const TcpclV4::tcpclv4_extensions_t & sessionExtensions)
        {
            ++m_numSessionInitCallbackCount;
            m_numSessionExtensionsProcessed += sessionExtensions.extensionsVec.size();
            BOOST_REQUIRE_EQUAL(m_keepAliveInterval, keepAliveIntervalSeconds);
            BOOST_REQUIRE_EQUAL(m_segmentMru, segmentMru);
            BOOST_REQUIRE_EQUAL(m_transferMru, transferMru);
            BOOST_REQUIRE_EQUAL(m_remoteNodeEidUri, remoteNodeEidUri);
            BOOST_REQUIRE(sessionExtensions == m_sessionExtensions);
            BOOST_REQUIRE(!(sessionExtensions != m_sessionExtensions));
        }
        /*
        void DataSegmentCallbackNoFragment(std::vector<uint8_t> & dataSegmentDataVec, bool isStartFlag, bool isEndFlag) {
            ++m_numDataSegmentCallbackCountNoFragment;
            BOOST_REQUIRE(isStartFlag);
            BOOST_REQUIRE(isEndFlag);
            const std::string rxBundleData(dataSegmentDataVec.data(), dataSegmentDataVec.data() + dataSegmentDataVec.size());
            BOOST_REQUIRE_EQUAL(m_bundleDataToSendNoFragment, rxBundleData);
        }

        void DataSegmentCallbackWithFragments(std::vector<uint8_t> & dataSegmentDataVec, bool isStartFlag, bool isEndFlag) {

            if (m_numDataSegmentCallbackCountWithFragments == 0) {
                BOOST_REQUIRE(isStartFlag);
                BOOST_REQUIRE(!isEndFlag);
            }
            else if (m_numDataSegmentCallbackCountWithFragments == 1) {
                BOOST_REQUIRE(!isStartFlag);
                BOOST_REQUIRE(!isEndFlag);
            }
            else if (m_numDataSegmentCallbackCountWithFragments == 2) {
                BOOST_REQUIRE(!isStartFlag);
                BOOST_REQUIRE(isEndFlag);
            }
            else {
                BOOST_REQUIRE(false);
            }
            ++m_numDataSegmentCallbackCountWithFragments;

            if (isStartFlag) {
                m_fragmentedBundleRxConcat.resize(0);
            }
            const std::string rxBundleData(dataSegmentDataVec.data(), dataSegmentDataVec.data() + dataSegmentDataVec.size());
            m_fragmentedBundleRxConcat.insert(m_fragmentedBundleRxConcat.end(), rxBundleData.begin(), rxBundleData.end()); //concatenate
        }*/

        void AckCallback(bool isStartSegment, bool isEndSegment, uint64_t transferId, uint64_t totalBytesAcknowledged) {
            ++m_numAckCallbackCount;
            BOOST_REQUIRE_EQUAL(m_ackIsStart, isStartSegment);
            BOOST_REQUIRE_EQUAL(m_ackIsEnd, isEndSegment);
            BOOST_REQUIRE_EQUAL(m_ackTransferId, transferId);
            BOOST_REQUIRE_EQUAL(m_ackBytesAcknowledged, totalBytesAcknowledged);
        }
        
        void BundleRefusalCallback(TCPCLV4_TRANSFER_REFUSE_REASON_CODES refusalCode, uint64_t transferId) {
            ++m_numBundleRefusalCallbackCount;
            BOOST_REQUIRE(refusalCode == TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_EXTENSION_FAILURE);
            BOOST_REQUIRE_EQUAL(m_bundleRefusalTransferId, transferId);
        }

        void MessageRejectCallback(TCPCLV4_MESSAGE_REJECT_REASON_CODES refusalCode, uint8_t rejectedMessageHeader) {
            ++m_numMessageRejectCallbackCount;
            BOOST_REQUIRE(refusalCode == TCPCLV4_MESSAGE_REJECT_REASON_CODES::MESSAGE_TYPE_UNKNOWN);
            BOOST_REQUIRE_EQUAL(rejectedMessageHeader, 253);
        }

        
        void KeepAliveCallback() {
            ++m_numKeepAliveCallbackCount;
        }

        void SessionTerminationMessageCallback(TCPCLV4_SESSION_TERMINATION_REASON_CODES terminationReasonCode, bool isAckOfAnEarlierSessionTerminationMessage) {
            ++m_numSessionTerminationMessageCallbackCount;
            BOOST_REQUIRE(m_sessionTerminationReasonCode == terminationReasonCode);
            BOOST_REQUIRE_EQUAL(m_isAckOfAnEarlierSessionTerminationMessage, isAckOfAnEarlierSessionTerminationMessage);
        }

    };
    
    Test t;

    //contact header with tls
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 0);
    t.m_useTls = true;
    t.DoRxContactHeader(); //use tls
    BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_tcpcl.InitRx();
    //contact header no tls
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    t.m_useTls = false;
    t.DoRxContactHeader(); //don't use tls
    BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 2);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    //session init
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 0);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 0);
    t.DoSessionInit();
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 1);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 0);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //add 1 extension
    t.m_sessionExtensions.extensionsVec.emplace_back(true, 10, std::vector<uint8_t>({ 0x5, 0x6 }));
    BOOST_REQUIRE(t.m_sessionExtensions.extensionsVec.back().IsCritical());
    t.DoSessionInit();
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 2);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //add another extension that is zero-length
    t.m_sessionExtensions.extensionsVec.emplace_back(true, 15, std::vector<uint8_t>());
    BOOST_REQUIRE(t.m_sessionExtensions.extensionsVec.back().IsCritical());
    t.DoSessionInit();
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 3);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 3);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //add a third extension
    t.m_sessionExtensions.extensionsVec.emplace_back(false, 20, std::vector<uint8_t>());
    BOOST_REQUIRE(!t.m_sessionExtensions.extensionsVec.back().IsCritical());
    t.DoSessionInit();
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 4);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 6);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    

    /*
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 0);
    t.DoDataSegmentNoFragmentCharByChar();
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    t.DoDataSegmentNoFragment();
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 2);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountWithFragments, 0);
    t.DoDataSegmentThreeFragments();
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountWithFragments, 3);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    */
    BOOST_REQUIRE_EQUAL(t.m_numAckCallbackCount, 0);
    t.m_ackIsStart = true;
    t.m_ackIsEnd = false;
    t.DoAck();
    BOOST_REQUIRE_EQUAL(t.m_numAckCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //repeat ack with start and end swapped
    t.m_ackIsStart = false;
    t.m_ackIsEnd = true;
    t.m_ackTransferId = 5555555555555;
    t.m_ackBytesAcknowledged = 888888888888;
    t.DoAck();
    BOOST_REQUIRE_EQUAL(t.m_numAckCallbackCount, 2);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    
    BOOST_REQUIRE_EQUAL(t.m_numBundleRefusalCallbackCount, 0);
    t.DoBundleRefusal();
    BOOST_REQUIRE_EQUAL(t.m_numBundleRefusalCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    BOOST_REQUIRE_EQUAL(t.m_numMessageRejectCallbackCount, 0);
    t.DoMessageReject();
    BOOST_REQUIRE_EQUAL(t.m_numMessageRejectCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    

    BOOST_REQUIRE_EQUAL(t.m_numKeepAliveCallbackCount, 0);
    t.DoKeepAlive();
    BOOST_REQUIRE_EQUAL(t.m_numKeepAliveCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    BOOST_REQUIRE_EQUAL(t.m_numSessionTerminationMessageCallbackCount, 0);
    t.m_sessionTerminationReasonCode = TCPCLV4_SESSION_TERMINATION_REASON_CODES::IDLE_TIMEOUT;
    t.m_isAckOfAnEarlierSessionTerminationMessage = true;
    t.DoSessionTermination();
    BOOST_REQUIRE_EQUAL(t.m_numSessionTerminationMessageCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE); //not contact header in v4 (needs to be able to receive non-bundles)
    //repeat with different values
    t.m_sessionTerminationReasonCode = TCPCLV4_SESSION_TERMINATION_REASON_CODES::BUSY;
    t.m_isAckOfAnEarlierSessionTerminationMessage = false;
    t.DoSessionTermination();
    BOOST_REQUIRE_EQUAL(t.m_numSessionTerminationMessageCallbackCount, 2);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

}



BOOST_AUTO_TEST_CASE(TcpclV4MagicHeaderStatesTestCase)
{

    TcpclV4 m_tcpcl;
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);
    m_tcpcl.HandleReceivedChar('c'); //not d.. remain in 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);
    m_tcpcl.HandleReceivedChar('d'); //first d.. advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('d'); //second d.. ddtn!.. remain
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('d'); //wrong but go to state 2 
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('v'); //wrong , back to 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    m_tcpcl.HandleReceivedChar('d'); //advance to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('d'); //wrong not !but go to state 2 
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('v'); //wrong not !, back to 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    m_tcpcl.HandleReceivedChar('d'); //advance to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('!'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION);
    m_tcpcl.HandleReceivedChar('d'); //wrong version.. back to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('v'); //wrong , back to 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    m_tcpcl.HandleReceivedChar('d'); //advance to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('!'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION);
    m_tcpcl.HandleReceivedChar(2); //wrong version.. back to 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    m_tcpcl.HandleReceivedChar('d'); //advance to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('!'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION);
    m_tcpcl.HandleReceivedChar(4); //right version.. advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_FLAGS);

    m_tcpcl.InitRx();
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    {
        const std::string bytesIn = "rrrrrrrrrrrrrdtyyyyyydtn!";
        m_tcpcl.HandleReceivedChars((const uint8_t *)bytesIn.c_str(), bytesIn.size());
        BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
        BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION);
    }
}
