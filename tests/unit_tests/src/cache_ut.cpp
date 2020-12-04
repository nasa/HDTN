#include <gtest/gtest.h>
#include <cache.hpp>
#include <iostream>
#include <arpa/inet.h>

// Create a test fixture
class CacheFixture : public testing::Test {
public:
    CacheFixture();
    ~CacheFixture();
    void SetUp() override;          // This is called after constructor.
    void TearDown() override;       // This is called before destructor.
    hdtn::flow_store * ptrFlowStore;
};

CacheFixture::CacheFixture() {
//    std::cout << "Called CacheFixture::CacheFixture()" << std::endl;
    ptrFlowStore = new hdtn::flow_store();
}

CacheFixture::~CacheFixture() {
//    std::cout << "Called CacheFixture::~CacheFixture()" << std::endl;
    delete ptrFlowStore;
}

void CacheFixture::SetUp() {
//    std::cout << "CacheFixture::SetUp called\n";
}

void CacheFixture::TearDown() {
//    std::cout << "CacheFixture::TearDown called\n";
}

TEST_F(CacheFixture,InitTestNominal1) {
    FAIL() << "Test needed for class flow_store, method init.";
}

TEST_F(CacheFixture,LoadTestNominal1) {
    FAIL() << "Test needed for class flow_store, method load.";
}

TEST_F(CacheFixture,WriteTestNominal1) {
    FAIL() << "Test needed for class flow_store, method write.";
}

TEST_F(CacheFixture,ReadTestNominal1) {
    FAIL() << "Test needed for class flow_store, method read.";
}

TEST_F(CacheFixture,StatsTestNominal1) {
    FAIL() << "Test needed for class flow_store, method stats.";
}

