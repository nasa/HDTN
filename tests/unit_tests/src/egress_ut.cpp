#include <gtest/gtest.h>
#include <egress.h>
#include <iostream>
#include <arpa/inet.h>

// Create a test fixture.  This text fixture also inherits from the class being tested so it can access the protected memebers.
class HegrUdpEntryFixture : public hdtn3::hegr_udp_entry, public testing::Test {
public:
    HegrUdpEntryFixture();
    ~HegrUdpEntryFixture();
    void SetUp() override;          // This is called after constructor.
    void TearDown() override;       // This is called before destructor.
};

HegrUdpEntryFixture::HegrUdpEntryFixture() {
//    std::cout << "Called HegrUdpEntryFixture::HegrUdpEntryFixture()" << std::endl;
}

HegrUdpEntryFixture::~HegrUdpEntryFixture() {
//    std::cout << "Called HegrUdpEntryFixture::~HegrUdpEntryFixture()" << std::endl;
}

void HegrUdpEntryFixture::SetUp() {
//    std::cout << "HegrUdpEntryFixture::SetUp called\n";
}

void HegrUdpEntryFixture::TearDown() {
//    std::cout << "HegrUdpEntryFixture::TearDown called\n";
}

// Create a test fixture.  This text fixture also inherits from the class being tested so it can access the protected memebers.
class HegrStcpEntryFixture : public hdtn3::hegr_stcp_entry, public testing::Test {
public:
    HegrStcpEntryFixture();
    ~HegrStcpEntryFixture();
    void SetUp() override;          // This is called after constructor.
    void TearDown() override;       // This is called before destructor.
};

HegrStcpEntryFixture::HegrStcpEntryFixture() {
//    std::cout << "Called HegrUdpEntryFixture::HegrUdpEntryFixture()" << std::endl;
}

HegrStcpEntryFixture::~HegrStcpEntryFixture() {
//    std::cout << "Called HegrStcpEntryFixture::~HegrUdpEntryFixture()" << std::endl;
}

void HegrStcpEntryFixture::SetUp() {
//    std::cout << "HegrStcpEntryFixture::SetUp called\n";
}

void HegrStcpEntryFixture::TearDown() {
//    std::cout << "HegrStcpEntryFixture::TearDown called\n";
}

TEST_F(HegrUdpEntryFixture,InitTestNominal1) {
    uint64_t flags = HEGR_FLAG_UDP;
    const char* dst = "127.0.0.1";
    int port = 4557;
    struct sockaddr_in saddr;
    saddr.sin_port = htons((uint16_t)port);
    saddr.sin_family = AF_INET;
    inet_pton(AF_INET, dst, &(saddr.sin_addr));
    this->init(&saddr,flags);
    EXPECT_EQ(saddr.sin_family,this->_ipv4.sin_family)  << "Problem HegrUdpEntryFixture, InitTestNominal1, comparison with sin_family";
    EXPECT_EQ(saddr.sin_port,this->_ipv4.sin_port)  << "Problem HegrUdpEntryFixture, InitTestNominal1, comparison with sin_port";
    EXPECT_EQ(saddr.sin_addr.s_addr,this->_ipv4.sin_addr.s_addr)  << "Problem HegrUdpEntryFixture, InitTestNominal1, comparison with sin_addr.s_addr";
}

TEST_F(HegrUdpEntryFixture,LabelTestNominal1) {
    this->label(1);
    EXPECT_EQ(1,this->_label);
}

// Test Driven Development.  Code currenly does not throw exception for negative value though it should.
// We can comment this test out until we are ready to add this feature.
//TEST_F(HegrUdpEntryFixture, LabelTestOffNominalNegativeValue)
//{
//    ASSERT_THROW(this->label(-1), std::runtime_error);
//}

// NOTE -- The method, name, takes a char *.  This should probably be a const char * as it is not meant to be mutable.  The code below gives us a warning.
TEST_F(HegrUdpEntryFixture,NameTestNominal1) {
    this->name("Test Name");
    EXPECT_STREQ("Test Name",this->_name);
}


TEST_F(HegrUdpEntryFixture,RateTestNominal1) {
    this->rate(1000);
    EXPECT_EQ(1000,this->_rate);
}

// Test Driven Devleopment.  Code currenly does not throw exception for negative value though it should.
// We can comment this test out until we are ready to add this feature.
//TEST_F(HegrUdpEntryFixture, RateTestOffNominalNegativeValue)
//{
//    ASSERT_THROW(this->rate(-1), std::runtime_error);
//}


TEST_F(HegrUdpEntryFixture,EnableTestNominal1) {
    uint64_t flags = HEGR_FLAG_UDP;
    const char* dst = "127.0.0.1";
    int port = 4557;
    struct sockaddr_in saddr;
    saddr.sin_port = htons((uint16_t)port);
    saddr.sin_family = AF_INET;
    inet_pton(AF_INET, dst, &(saddr.sin_addr));
    this->init(&saddr,flags);
    int returnCode = this->enable();
    EXPECT_EQ(0,returnCode) << "Problem HegrUdpEntryFixture, EnableTestNominal1, comparison with returnCode";
    EXPECT_EQ(HEGR_FLAG_UP,this->_flags & HEGR_FLAG_UP) << "Problem HegrUdpEntryFixture, EnableTestNominal1, comparison with _flags";
}


// It does not seem right that this test is valid.
TEST_F(HegrUdpEntryFixture,EnableTestOffNominal1) {
    // Calling enable without first calling init
    int returnCode = this->enable();
    EXPECT_EQ(0,returnCode) << "Problem HegrUdpEntryFixture, EnableTestNominal1, comparison with returnCode";
    EXPECT_EQ(HEGR_FLAG_UP,this->_flags & HEGR_FLAG_UP) << "Problem HegrUdpEntryFixture, EnableTestNominal1, comparison with _flags";
}

TEST_F(HegrUdpEntryFixture,DisableTestNominal1) {
    this->enable();
    int returnCode = this->disable();
    EXPECT_EQ(0,returnCode) << "Problem HegrUdpEntryFixture, DisableTestNominal1, comparison with returnCode";
    EXPECT_NE(HEGR_FLAG_UP,this->_flags & HEGR_FLAG_UP) << "Problem HegrUdpEntryFixture, DisableTestNominal1, comparison with _flags";
}

//TEST_F(HegrUdpEntryFixture,ForwardTestNominal1) {
//    this->enable();
//    int returnCode = this->disable();
//    EXPECT_EQ(0,returnCode) << "Problem HegrUdpEntryFixture, DisableTestNominal1, comparison with returnCode";
//    EXPECT_NE(HEGR_FLAG_UP,this->_flags & HEGR_FLAG_UP) << "Problem HegrUdpEntryFixture, DisableTestNominal1, comparison with _flags";
//}

TEST_F(HegrUdpEntryFixture,ForwardTestNominal1) {
    FAIL() << "Test needed for class hegr_udp_entry, method forward.";
}

TEST_F(HegrUdpEntryFixture,UpdateTestNominal1) {
    FAIL() << "Test needed for class hegr_udp_entry, method update.";
}


TEST_F(HegrStcpEntryFixture,InitTestNominal1) {
    FAIL() << "Test needed for class hegr_stcp_entry, method init.";
}

TEST_F(HegrStcpEntryFixture,RateTestNominal1) {
    FAIL() << "Test needed for class hegr_stcp_entry, method rate.";
}

TEST_F(HegrStcpEntryFixture,ForwardTestNominal1) {
    FAIL() << "Test needed for class hegr_stcp_entry, method forward.";
}

TEST_F(HegrStcpEntryFixture,UpdateTestNominal1) {
    FAIL() << "Test needed for class hegr_stcp_entry, method update.";
}

TEST_F(HegrStcpEntryFixture,EnableTestNominal1) {
    FAIL() << "Test needed for class hegr_stcp_entry, method enable.";
}

TEST_F(HegrStcpEntryFixture,DisableTestNominal1) {
    FAIL() << "Test needed for class hegr_stcp_entry, method disable.";
}

