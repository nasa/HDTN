/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
*/

#include <iostream>
#include <string>
#include "BundleStorageManagerMT.h"
#include "BundleStorageManagerAsio.h"
#include <boost/make_unique.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/timer/timer.hpp>
#include "SignalHandler.h"

static const uint64_t PRIMARY_SRC_NODE = 100;
static const uint64_t PRIMARY_SRC_SVC = 1;
//static const uint64_t PRIMARY_DEST_NODE = 300;
//static const uint64_t PRIMARY_DEST_SVC = 3;
//static const uint64_t PRIMARY_TIME = 1000;
//static const uint64_t PRIMARY_LIFETIME = 2000;
static const uint64_t PRIMARY_SEQ = 1;

static volatile bool g_running = true;

static void MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    hdtn::Logger::getInstance()->logNotification("storage", "Keyboard Interrupt.. exiting");
    g_running = false; //do this first
}

static SignalHandler g_sigHandler(boost::bind(&MonitorExitKeypressThreadFunction));



struct TestFile {
    TestFile() {}
    TestFile(boost::uint64_t size) : m_data(size) {
        boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
        const boost::random::uniform_int_distribution<> distRandomData(0, 255);
        for (std::size_t i = 0; i < size; ++i) {
            m_data[i] = distRandomData(gen);
        }
    }
    std::vector<boost::uint8_t> m_data;
};

//two days
#define NUMBER_OF_EXPIRATIONS (86400*2)

bool TestSpeed(BundleStorageManagerBase & bsm) {
    boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
    const boost::random::uniform_int_distribution<> distLinkId(0, 9);
    const boost::random::uniform_int_distribution<> distFileId(0, 9);
    const boost::random::uniform_int_distribution<> distPriorityIndex(0, 2);
    const boost::random::uniform_int_distribution<> distAbsExpiration(0, NUMBER_OF_EXPIRATIONS - 1);
    const boost::random::uniform_int_distribution<> distTotalBundleSize(1, 65536);

    g_sigHandler.Start();

    static const cbhe_eid_t DEST_LINKS[10] = {
        cbhe_eid_t(1,1),
        cbhe_eid_t(2,1),
        cbhe_eid_t(3,1),
        cbhe_eid_t(4,1),
        cbhe_eid_t(5,1),
        cbhe_eid_t(6,1),
        cbhe_eid_t(7,1),
        cbhe_eid_t(8,1),
        cbhe_eid_t(9,1),
        cbhe_eid_t(10,1)
    };
    const std::vector<cbhe_eid_t> availableDestLinks = {
        cbhe_eid_t(1,1),
        cbhe_eid_t(2,1),
        cbhe_eid_t(3,1),
        cbhe_eid_t(4,1),
        cbhe_eid_t(5,1),
        cbhe_eid_t(6,1),
        cbhe_eid_t(7,1),
        cbhe_eid_t(8,1),
        cbhe_eid_t(9,1),
        cbhe_eid_t(10,1)
    };



    bsm.Start();

    

    static const boost::uint64_t sizes[10] = {

        BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

        2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

        500 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        500 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

        1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

        10000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        10000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,
    };
    std::cout << "generating test files\n";
    hdtn::Logger::getInstance()->logNotification("storage", "Generating test files");
    std::vector<TestFile> testFiles(10);
    std::map<boost::uint64_t, TestFile*> fileMap;
    for (std::size_t i = 0; i < 10; ++i) {
        testFiles[i] = TestFile(sizes[i]);
        fileMap[sizes[i]] = &testFiles[i];
    }
    std::cout << "done generating test files\n";
    hdtn::Logger::getInstance()->logNotification("storage", "Done generating test files");

    boost::uint64_t totalSegmentsStoredOnDisk = 0;
    double gigaBitsPerSecReadDoubleAvg = 0.0, gigaBitsPerSecWriteDoubleAvg = 0.0;
    const unsigned int NUM_TESTS = 5;
    uint64_t custodyId = 0;
    for (unsigned int testI = 0; testI < NUM_TESTS; ++testI) {

        {
            std::cout << "filling up the storage\n";
            hdtn::Logger::getInstance()->logNotification("storage", "Filling up the storage");
            boost::uint64_t totalBytesWrittenThisTest = 0;
            boost::timer::cpu_timer timer;
            while (g_running) {
                const unsigned int fileIdx = distFileId(gen);
                std::vector<boost::uint8_t> & data = testFiles[fileIdx].m_data;
                const boost::uint64_t size = data.size();
                //std::cout << "testing size " << size << "\n";

                const unsigned int linkId = distLinkId(gen);
                const unsigned int priorityIndex = distPriorityIndex(gen);
                const uint64_t absExpiration = distAbsExpiration(gen);

                BundleStorageManagerSession_WriteToDisk sessionWrite;
                bpv6_primary_block primary;
                primary.SetZero();
                primary.flags = bpv6_bundle_set_priority(priorityIndex) |
                    bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT);
                primary.src_node = PRIMARY_SRC_NODE;
                primary.src_svc = PRIMARY_SRC_SVC;
                primary.dst_node = DEST_LINKS[linkId].nodeId;
                primary.dst_svc = DEST_LINKS[linkId].serviceId;
                primary.custodian_node = 0;
                primary.custodian_svc = 0;
                primary.creation = 0;
                primary.lifetime = absExpiration;
                primary.sequence = PRIMARY_SEQ;

                boost::uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, primary, size);
                //std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
                if (totalSegmentsRequired == 0) break;
                totalSegmentsStoredOnDisk += totalSegmentsRequired;
                totalBytesWrittenThisTest += size;

                ++custodyId;
                const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, primary, custodyId, data.data(), data.size());
                
            }
            const boost::uint64_t nanoSecWall = timer.elapsed().wall;
            //std::cout << "nanosec=" << nanoSecWall << "\n";
            const double bytesPerNanoSecDouble = static_cast<double>(totalBytesWrittenThisTest) / static_cast<double>(nanoSecWall);
            const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
            //std::cout << "GBytes/sec=" << gigaBytesPerSecDouble << "\n";
            const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
            gigaBitsPerSecWriteDoubleAvg += gigaBitsPerSecDouble;
            std::cout << "WRITE GBits/sec=" << gigaBitsPerSecDouble << "\n\n";
            hdtn::Logger::getInstance()->logInfo("storage", "WRITE GBits/sec=" + std::to_string(gigaBitsPerSecDouble));
        }
        {
            std::cout << "reading half of the stored\n";
            hdtn::Logger::getInstance()->logNotification("storage", "Reading half of the stored");
            boost::uint64_t totalBytesReadThisTest = 0;
            boost::timer::cpu_timer timer;
            BundleStorageManagerSession_ReadFromDisk sessionRead; //reuse (heap allocated)
            while (g_running) {

                
                boost::uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
                //std::cout << "bytesToReadFromDisk " << bytesToReadFromDisk << "\n";
                std::vector<boost::uint8_t> dataReadBack(bytesToReadFromDisk);
                TestFile & originalFile = *fileMap[bytesToReadFromDisk];

                const std::size_t numSegmentsToRead = sessionRead.catalogEntryPtr->segmentIdChainVec.size();
                bsm.ReadAllSegments(sessionRead, dataReadBack);
                const std::size_t totalBytesRead = dataReadBack.size();
                
                //std::cout << "totalBytesRead " << totalBytesRead << "\n";
                if (totalBytesRead != bytesToReadFromDisk) return false;
                totalBytesReadThisTest += totalBytesRead;
                if (dataReadBack != originalFile.m_data) {
                    std::cout << "dataReadBack does not equal data\n";
                    hdtn::Logger::getInstance()->logWarning("storage", "dataReadBack does not equal data");
                    return false;
                }
                if (!bsm.RemoveReadBundleFromDisk(sessionRead)) {
                    std::cout << "error freeing bundle from disk\n";
                    hdtn::Logger::getInstance()->logError("storage", "Error freeing bundle from disk");
                    return false;
                }

                totalSegmentsStoredOnDisk -= numSegmentsToRead;
                if (totalSegmentsStoredOnDisk < (bsm.M_MAX_SEGMENTS / 2)) {
                    break;
                }
            }
            const boost::uint64_t nanoSecWall = timer.elapsed().wall;
            //std::cout << "nanosec=" << nanoSecWall << "\n";
            const double bytesPerNanoSecDouble = static_cast<double>(totalBytesReadThisTest) / static_cast<double>(nanoSecWall);
            const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
            //std::cout << "GBytes/sec=" << gigaBytesPerSecDouble << "\n";
            const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
            gigaBitsPerSecReadDoubleAvg += gigaBitsPerSecDouble;
            std::cout << "READ GBits/sec=" << gigaBitsPerSecDouble << "\n\n";
            hdtn::Logger::getInstance()->logInfo("storage", "READ GBits/sec=" + std::to_string(gigaBitsPerSecDouble));
        }
    }

    if (g_running) {
        std::cout << "Read avg GBits/sec=" << gigaBitsPerSecReadDoubleAvg / NUM_TESTS << "\n\n";
        std::cout << "Write avg GBits/sec=" << gigaBitsPerSecWriteDoubleAvg / NUM_TESTS << "\n\n";
        hdtn::Logger::getInstance()->logInfo("storage", "Read avg GBits/sec=" + std::to_string(gigaBitsPerSecReadDoubleAvg));
        hdtn::Logger::getInstance()->logInfo("storage", "Write avg GBits/sec=" + std::to_string(gigaBitsPerSecWriteDoubleAvg));
    }
    return true;

}


int main() {
    std::unique_ptr<BundleStorageManagerBase> bsmPtr;
    if (true) {
        boost::make_unique<BundleStorageManagerMT>();
    }
    else {
        boost::make_unique<BundleStorageManagerAsio>();
    }
    std::cout << TestSpeed(*bsmPtr) << "\n";
    return 0;
}
