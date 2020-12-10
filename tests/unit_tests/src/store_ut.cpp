#include <gtest/gtest.h>
#include <store.hpp>
#include <iostream>
#include <arpa/inet.h>
#include <sys/time.h>


// Create a test fixture
class StorageFixture : public testing::Test {
public:
    StorageFixture();
    ~StorageFixture();
    void SetUp() override;          // This is called after constructor.
    void TearDown() override;       // This is called before destructor.
    
    hdtn::storage * ptrStorage;
    hdtn::storage_worker * ptrStorageWorker;
    hdtn::scheduler * ptrScheduler;
};

StorageFixture::StorageFixture() {
//    std::cout << "Called StorageFixture::StorageFixture()" << std::endl;
    ptrStorage = new hdtn::storage();
    ptrStorageWorker = new hdtn::storage_worker();
    ptrScheduler = new hdtn::scheduler();
}

StorageFixture::~StorageFixture() {
//    std::cout << "Called StorageFixture::~StorageFixture()" << std::endl;
    delete ptrStorage;
    delete ptrStorageWorker;
    delete ptrScheduler;
}

void StorageFixture::SetUp() {
//    std::cout << "StorageFixture::SetUp called\n";
}

void StorageFixture::TearDown() {
//    std::cout << "StorageFixture::TearDown called\n";
}


TEST_F(StorageFixture,DISABLED_InitTestNominal1) {
\
    double last = 0.0;
    timeval tv;
    gettimeofday(&tv, NULL);
    last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    hdtn::storage_config config;
    config.regsvr = "tcp://127.0.0.1:10140";
    config.local = "tcp://127.0.0.1:10145";
    config.store_path = "/var/lib/hdtn.store";
    hdtn::storage store;
    std::cout << "[store] Initializing storage manager ..." << std::endl;

    bool returnValue = store.init(config);

    ASSERT_EQ(true,returnValue);
\
}

TEST_F(StorageFixture,DISABLED_UpdateTestNominal1) {
    FAIL() << "Test needed for class storage, method update.";
}

TEST_F(StorageFixture,DISABLED_DispatchTestNominal1) {
    FAIL() << "Test needed for class storage, method dispatch.";
}

TEST_F(StorageFixture,DISABLED_C2TelemTestNominal1) {
    FAIL() << "Test needed for class storage, method c2telem.";
}

TEST_F(StorageFixture,DISABLED_ReleaseTestNominal1) {
    FAIL() << "Test needed for class storage, method release.";
}

TEST_F(StorageFixture,DISABLED_IngressTestNominal1) {
    FAIL() << "Test needed for class storage, method ingress.";
}

TEST_F(StorageFixture,DISABLED_StatsTestNominal1) {
    FAIL() << "Test needed for class storage, method stats.";
}

TEST_F(StorageFixture,DISABLED_InitWorkerTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method init.";
}

TEST_F(StorageFixture,DISABLED_LaunchTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method launch.";
}

TEST_F(StorageFixture,DISABLED_ExecuteTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method execute.";
}

TEST_F(StorageFixture,DISABLED_ThreadTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method thread.";
}

TEST_F(StorageFixture,DISABLED_WriteTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method write.";
}

TEST_F(StorageFixture,DISABLED_InitSchedulerTestNominal1) {
    FAIL() << "Test needed for class scheduler, method init.";
}

TEST_F(StorageFixture,DISABLED_AddTestNominal1) {
    FAIL() << "Test needed for class scheduler, method add.";
}

TEST_F(StorageFixture,DISABLED_NextTestNominal1) {
    FAIL() << "Test needed for class scheduler, method next.";
}


