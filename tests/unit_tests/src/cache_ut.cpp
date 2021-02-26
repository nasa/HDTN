
#include <arpa/inet.h>
#include <cache.hpp>
#include <iostream>
#include <boost/test/unit_test.hpp>

// Create a test fixture
class CacheFixture  {
public:
    CacheFixture();
    ~CacheFixture();
private:
    hdtn::flow_store* ptrFlowStore;
};

CacheFixture::CacheFixture() {
    std::cout << "Called CacheFixture::CacheFixture()" << std::endl;
    ptrFlowStore = new hdtn::flow_store();
}

CacheFixture::~CacheFixture() {
    std::cout << "Called CacheFixture::~CacheFixture()" << std::endl;
    delete ptrFlowStore;
}


//TEST_F(CacheFixture, DISABLED_InitTestNominal1) { FAIL() << "Test needed for class flow_store, method init."; }

//TEST_F(CacheFixture, DISABLED_LoadTestNominal1) { FAIL() << "Test needed for class flow_store, method load."; }

//TEST_F(CacheFixture, DISABLED_WriteTestNominal1) { FAIL() << "Test needed for class flow_store, method write."; }

//TEST_F(CacheFixture, DISABLED_ReadTestNominal1) { FAIL() << "Test needed for class flow_store, method read."; }

//TEST_F(CacheFixture, DISABLED_StatsTestNominal1) { FAIL() << "Test needed for class flow_store, method stats."; }


