#include <boost/test/unit_test.hpp>
#include "Tcpcl.h"
#include <boost/bind.hpp>



BOOST_AUTO_TEST_CASE(TcpclFullTestCase)
{
	struct Test {
		Tcpcl m_tcpcl;
		const CONTACT_HEADER_FLAGS m_contactHeaderFlags;
		const uint16_t m_keepAliveInterval;
		const std::string m_localEidStr;
		const std::string m_bundleDataToSendNoFragment;
		unsigned int m_numContactHeaderCallbackCount;
		unsigned int m_numDataSegmentCallbackCountNoFragment;
		unsigned int m_numDataSegmentCallbackCountWithFragments;
		unsigned int m_numAckCallbackCount;
		unsigned int m_numBundleRefusalCallbackCount;
		unsigned int m_numBundleLengthCallbackCount;
		unsigned int m_numKeepAliveCallbackCount;
		unsigned int m_numShutdownCallbacksWithReasonWithDelay;
		unsigned int m_numShutdownCallbacksNoReasonNoDelay;
		unsigned int m_numShutdownCallbacksWithReasonNoDelay;
		unsigned int m_numShutdownCallbacksNoReasonWithDelay;
		std::string m_fragmentedBundleRxConcat;
		Test() :
			m_contactHeaderFlags(CONTACT_HEADER_FLAGS::SUPPORT_BUNDLE_REFUSAL),
			m_keepAliveInterval(0x1234),
			m_localEidStr("test Eid String!"),
			m_bundleDataToSendNoFragment("this is a test bundle"),
			m_numContactHeaderCallbackCount(0),
			m_numDataSegmentCallbackCountNoFragment(0),
			m_numDataSegmentCallbackCountWithFragments(0),
			m_numAckCallbackCount(0),
			m_numBundleRefusalCallbackCount(0),
			m_numBundleLengthCallbackCount(0),
			m_numKeepAliveCallbackCount(0),
			m_numShutdownCallbacksWithReasonWithDelay(0),
			m_numShutdownCallbacksNoReasonNoDelay(0),
			m_numShutdownCallbacksWithReasonNoDelay(0),
			m_numShutdownCallbacksNoReasonWithDelay(0),
			m_fragmentedBundleRxConcat("")
		{
			
		}
		
		void DoRxContactHeader() {
			m_tcpcl.SetContactHeaderReadCallback(boost::bind(&Test::ContactHeaderCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));

			std::vector<uint8_t> hdr;
			Tcpcl::GenerateContactHeader(hdr, m_contactHeaderFlags, m_keepAliveInterval, m_localEidStr);
			m_tcpcl.HandleReceivedChars(hdr.data(), hdr.size());
		}

		void DoAck() {
			m_tcpcl.SetAckSegmentReadCallback(boost::bind(&Test::AckCallback, this, boost::placeholders::_1));

			std::vector<uint8_t> ackSegment;
			Tcpcl::GenerateAckSegment(ackSegment, 0x1234567f);
			m_tcpcl.HandleReceivedChars(ackSegment.data(), ackSegment.size());
		}

		void DoBundleRefusal() {
			m_tcpcl.SetBundleRefusalCallback(boost::bind(&Test::BundleRefusalCallback, this, boost::placeholders::_1));

			std::vector<uint8_t> bundleRefusalSegment;
			Tcpcl::GenerateBundleRefusal(bundleRefusalSegment, BUNDLE_REFUSAL_CODES::RECEIVER_RESOURCES_EXHAUSTED);
			m_tcpcl.HandleReceivedChars(bundleRefusalSegment.data(), bundleRefusalSegment.size());
		}

		void DoNextBundleLength() {
			m_tcpcl.SetNextBundleLengthCallback(boost::bind(&Test::NextBundleLengthCallback, this, boost::placeholders::_1));

			std::vector<uint8_t> nextBundleLengthSegment;
			Tcpcl::GenerateBundleLength(nextBundleLengthSegment, 0xdeadbeef);
			m_tcpcl.HandleReceivedChars(nextBundleLengthSegment.data(), nextBundleLengthSegment.size());
		}

		void DoKeepAlive() {
			m_tcpcl.SetKeepAliveCallback(boost::bind(&Test::KeepAliveCallback, this));

			std::vector<uint8_t> keepAliveSegment;
			Tcpcl::GenerateKeepAliveMessage(keepAliveSegment);
			m_tcpcl.HandleReceivedChars(keepAliveSegment.data(), keepAliveSegment.size());
		}

		void DoShutdownWithReasonWithDelay() {
			m_tcpcl.SetShutdownMessageCallback(boost::bind(&Test::ShutdownCallbackWithReasonWithDelay, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));

			std::vector<uint8_t> shutdownSegment;
			Tcpcl::GenerateShutdownMessage(shutdownSegment, true, SHUTDOWN_REASON_CODES::BUSY, true, 0x76543210);
			m_tcpcl.HandleReceivedChars(shutdownSegment.data(), shutdownSegment.size());
		}

		void DoShutdownNoReasonNoDelay() {
			m_tcpcl.SetShutdownMessageCallback(boost::bind(&Test::ShutdownCallbackNoReasonNoDelay, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));

			std::vector<uint8_t> shutdownSegment;
			Tcpcl::GenerateShutdownMessage(shutdownSegment, false, SHUTDOWN_REASON_CODES::UNASSIGNED, false, 0);
			m_tcpcl.HandleReceivedChars(shutdownSegment.data(), shutdownSegment.size());
		}

		void DoShutdownWithReasonNoDelay() {
			m_tcpcl.SetShutdownMessageCallback(boost::bind(&Test::ShutdownCallbackWithReasonNoDelay, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));

			std::vector<uint8_t> shutdownSegment;
			Tcpcl::GenerateShutdownMessage(shutdownSegment, true, SHUTDOWN_REASON_CODES::IDLE_TIMEOUT, false, 0);
			m_tcpcl.HandleReceivedChars(shutdownSegment.data(), shutdownSegment.size());
		}

		void DoShutdownNoReasonWithDelay() {
			m_tcpcl.SetShutdownMessageCallback(boost::bind(&Test::ShutdownCallbackNoReasonWithDelay, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));

			std::vector<uint8_t> shutdownSegment;
			Tcpcl::GenerateShutdownMessage(shutdownSegment, false, SHUTDOWN_REASON_CODES::UNASSIGNED, true, 0x98765432);
			m_tcpcl.HandleReceivedChars(shutdownSegment.data(), shutdownSegment.size());
		}

		void DoDataSegmentNoFragment() {
			std::vector<uint8_t> bundleSegment;
			m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&Test::DataSegmentCallbackNoFragment, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
			Tcpcl::GenerateDataSegment(bundleSegment, true, true, (const uint8_t*)m_bundleDataToSendNoFragment.data(), m_bundleDataToSendNoFragment.size());
			m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());
		}

		void DoDataSegmentThreeFragments() {
			m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&Test::DataSegmentCallbackWithFragments, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
			std::vector<uint8_t> bundleSegment;

			BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
			BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, std::string(""));
			BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 0);
			static const std::string f1 = "fragOne ";
			Tcpcl::GenerateDataSegment(bundleSegment, true, false, (const uint8_t*)f1.data(), f1.size());
			m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());

			BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
			BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, f1);
			BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 1);
			static const std::string f2 = "fragTwo ";
			Tcpcl::GenerateDataSegment(bundleSegment, false, false, (const uint8_t*)f2.data(), f2.size());
			m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());

			BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
			BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, f1 + f2);
			BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 2);
			static const std::string f3 = "fragThree";
			Tcpcl::GenerateDataSegment(bundleSegment, false, true, (const uint8_t*)f3.data(), f3.size());
			m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());

			BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
			BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, f1+f2+f3);
			BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 3);
		}

		void ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid) {
			++m_numContactHeaderCallbackCount;
			BOOST_REQUIRE(m_contactHeaderFlags == flags);
			BOOST_REQUIRE_EQUAL(m_keepAliveInterval, keepAliveIntervalSeconds);
			BOOST_REQUIRE_EQUAL(m_localEidStr, localEid);
		}

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
		}

		void AckCallback(uint64_t totalBytesAcknowledged) {
			++m_numAckCallbackCount;
			BOOST_REQUIRE_EQUAL(0x1234567F, totalBytesAcknowledged);
		}

		void BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode) {
			++m_numBundleRefusalCallbackCount;
			BOOST_REQUIRE(refusalCode == BUNDLE_REFUSAL_CODES::RECEIVER_RESOURCES_EXHAUSTED);
		}

		void NextBundleLengthCallback(uint64_t nextBundleLength) {
			++m_numBundleLengthCallbackCount;
			BOOST_REQUIRE_EQUAL(0xdeadbeef, nextBundleLength);
		}

		void KeepAliveCallback() {
			++m_numKeepAliveCallbackCount;
		}

		void ShutdownCallbackWithReasonWithDelay(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
												 bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds)
		{
			++m_numShutdownCallbacksWithReasonWithDelay;
			BOOST_REQUIRE(hasReasonCode);
			BOOST_REQUIRE(hasReconnectionDelay);
			BOOST_REQUIRE(SHUTDOWN_REASON_CODES::BUSY == shutdownReasonCode);
			BOOST_REQUIRE_EQUAL(reconnectionDelaySeconds, 0x76543210);
			
		}

		void ShutdownCallbackNoReasonNoDelay(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
												 bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds)
		{
			++m_numShutdownCallbacksNoReasonNoDelay;
			BOOST_REQUIRE(!hasReasonCode);
			BOOST_REQUIRE(!hasReconnectionDelay);
		}

		void ShutdownCallbackWithReasonNoDelay(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
												 bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds)
		{
			++m_numShutdownCallbacksWithReasonNoDelay;
			BOOST_REQUIRE(hasReasonCode);
			BOOST_REQUIRE(!hasReconnectionDelay);
			BOOST_REQUIRE(SHUTDOWN_REASON_CODES::IDLE_TIMEOUT == shutdownReasonCode);
		}

		void ShutdownCallbackNoReasonWithDelay(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
												 bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds)
		{
			++m_numShutdownCallbacksNoReasonWithDelay;
			BOOST_REQUIRE(!hasReasonCode);
			BOOST_REQUIRE(hasReconnectionDelay);
			BOOST_REQUIRE_EQUAL(reconnectionDelaySeconds, 0x98765432);

		}
	};

	Test t;

	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 0);
	t.DoRxContactHeader();
	BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

	BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 0);
	t.DoDataSegmentNoFragment();
	BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

	BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountWithFragments, 0);
	t.DoDataSegmentThreeFragments();
	BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountWithFragments, 3);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

	BOOST_REQUIRE_EQUAL(t.m_numAckCallbackCount, 0);
	t.DoAck();
	BOOST_REQUIRE_EQUAL(t.m_numAckCallbackCount, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
	
	BOOST_REQUIRE_EQUAL(t.m_numBundleRefusalCallbackCount, 0);
	t.DoBundleRefusal();
	BOOST_REQUIRE_EQUAL(t.m_numBundleRefusalCallbackCount, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

	BOOST_REQUIRE_EQUAL(t.m_numBundleLengthCallbackCount, 0);
	t.DoNextBundleLength();
	BOOST_REQUIRE_EQUAL(t.m_numBundleLengthCallbackCount, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
	
	BOOST_REQUIRE_EQUAL(t.m_numKeepAliveCallbackCount, 0);
	t.DoKeepAlive();
	BOOST_REQUIRE_EQUAL(t.m_numKeepAliveCallbackCount, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

	BOOST_REQUIRE_EQUAL(t.m_numShutdownCallbacksWithReasonWithDelay, 0);
	t.DoShutdownWithReasonWithDelay();
	BOOST_REQUIRE_EQUAL(t.m_numShutdownCallbacksWithReasonWithDelay, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);


	//reconnect
	BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 1);
	t.DoRxContactHeader();
	BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 2);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

	//shutdown
	BOOST_REQUIRE_EQUAL(t.m_numShutdownCallbacksNoReasonNoDelay, 0);
	t.DoShutdownNoReasonNoDelay();
	BOOST_REQUIRE_EQUAL(t.m_numShutdownCallbacksNoReasonNoDelay, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);

	//reconnect
	BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 2);
	t.DoRxContactHeader();
	BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 3);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

	//shutdown
	BOOST_REQUIRE_EQUAL(t.m_numShutdownCallbacksWithReasonNoDelay, 0);
	t.DoShutdownWithReasonNoDelay();
	BOOST_REQUIRE_EQUAL(t.m_numShutdownCallbacksWithReasonNoDelay, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);

	//reconnect
	BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 3);
	t.DoRxContactHeader();
	BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 4);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

	//shutdown
	BOOST_REQUIRE_EQUAL(t.m_numShutdownCallbacksNoReasonWithDelay, 0);
	t.DoShutdownNoReasonWithDelay();
	BOOST_REQUIRE_EQUAL(t.m_numShutdownCallbacksNoReasonWithDelay, 1);
	BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
}



BOOST_AUTO_TEST_CASE(TcpclMagicHeaderStatesTestCase)
{

	Tcpcl m_tcpcl;
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1);
	m_tcpcl.HandleReceivedChar('c'); //not d.. remain in 1
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1);
	m_tcpcl.HandleReceivedChar('d'); //first d.. advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
	m_tcpcl.HandleReceivedChar('d'); //second d.. ddtn!.. remain
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
	m_tcpcl.HandleReceivedChar('t'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
	m_tcpcl.HandleReceivedChar('d'); //wrong but go to state 2 
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
	m_tcpcl.HandleReceivedChar('t'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
	m_tcpcl.HandleReceivedChar('v'); //wrong , back to 1
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

	m_tcpcl.HandleReceivedChar('d'); //advance to 2
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
	m_tcpcl.HandleReceivedChar('t'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
	m_tcpcl.HandleReceivedChar('n'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
	m_tcpcl.HandleReceivedChar('d'); //wrong not !but go to state 2 
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
	m_tcpcl.HandleReceivedChar('t'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
	m_tcpcl.HandleReceivedChar('n'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
	m_tcpcl.HandleReceivedChar('v'); //wrong not !, back to 1
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

	m_tcpcl.HandleReceivedChar('d'); //advance to 2
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
	m_tcpcl.HandleReceivedChar('t'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
	m_tcpcl.HandleReceivedChar('n'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
	m_tcpcl.HandleReceivedChar('!'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_VERSION);
	m_tcpcl.HandleReceivedChar('d'); //wrong version.. back to 2
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
	m_tcpcl.HandleReceivedChar('v'); //wrong , back to 1
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

	m_tcpcl.HandleReceivedChar('d'); //advance to 2
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
	m_tcpcl.HandleReceivedChar('t'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
	m_tcpcl.HandleReceivedChar('n'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
	m_tcpcl.HandleReceivedChar('!'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_VERSION);
	m_tcpcl.HandleReceivedChar(2); //wrong version.. back to 1
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

	m_tcpcl.HandleReceivedChar('d'); //advance to 2
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
	m_tcpcl.HandleReceivedChar('t'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
	m_tcpcl.HandleReceivedChar('n'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
	m_tcpcl.HandleReceivedChar('!'); //advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_VERSION);
	m_tcpcl.HandleReceivedChar(3); //right version.. advance
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_FLAGS);

	m_tcpcl.InitRx();
	BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
	BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

	{
		const std::string bytesIn = "rrrrrrrrrrrrrdtyyyyyydtn!";
		m_tcpcl.HandleReceivedChars((const uint8_t *)bytesIn.c_str(), bytesIn.size());
		BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER);
		BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_VERSION);
	}
}
