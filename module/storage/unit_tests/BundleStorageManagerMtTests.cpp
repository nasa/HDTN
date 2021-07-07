#include <boost/test/unit_test.hpp>
#include "BundleStorageManagerMT.h"
#include "BundleStorageManagerAsio.h"
#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/timer/timer.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include "SignalHandler.h"
#include "Environment.h"

BOOST_AUTO_TEST_CASE(BundleStorageManagerAllTestCase)
{
    for (unsigned int whichBsm = 0; whichBsm < 2; ++whichBsm) {
        boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
        const boost::random::uniform_int_distribution<> distRandomData(0, 255);
        const boost::random::uniform_int_distribution<> distLinkId(0, 9);
        const boost::random::uniform_int_distribution<> distPriorityIndex(0, 2);
        const boost::random::uniform_int_distribution<> distAbsExpiration(0, NUMBER_OF_EXPIRATIONS - 1);


        static const boost::uint64_t DEST_LINKS[10] = { 1,2,3,4,5,6,7,8,9,10 };
        const std::vector<boost::uint64_t> availableDestLinks = { 1,2,3,4,5,6,7,8,9,10 };



        std::unique_ptr<BundleStorageManagerBase> bsmPtr;
        StorageConfig_ptr ptrStorageConfig = StorageConfig::CreateFromJsonFile((Environment::GetPathHdtnSourceRoot() / "tests" / "config_files" / "storage" / "storageConfigRelativePaths.json").string());
        ptrStorageConfig->m_tryToRestoreFromDisk = false; //manually set this json entry
        ptrStorageConfig->m_autoDeleteFilesOnExit = true; //manually set this json entry
        if (whichBsm == 0) {
            std::cout << "create BundleStorageManagerMT" << std::endl;
            bsmPtr = boost::make_unique<BundleStorageManagerMT>(ptrStorageConfig);
        }
        else {
            std::cout << "create BundleStorageManagerAsio" << std::endl;
            bsmPtr = boost::make_unique<BundleStorageManagerAsio>(ptrStorageConfig);
        }
        BundleStorageManagerBase & bsm = *bsmPtr;

        bsm.Start();


        static const boost::uint64_t sizes[17] = {
            1,
            2,
            BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
            BUNDLE_STORAGE_PER_SEGMENT_SIZE - 1 ,
            BUNDLE_STORAGE_PER_SEGMENT_SIZE - 0,
            BUNDLE_STORAGE_PER_SEGMENT_SIZE + 1,
            BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 1 ,
            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 0,
            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 1,
            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 1 ,
            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 0,
            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 1,
            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,
        };
        for (unsigned int sizeI = 0; sizeI < 17; ++sizeI) {
            const boost::uint64_t size = sizes[sizeI];
            //std::cout << "testing size " << size << "\n";
            std::vector<boost::uint8_t> data(size);
            std::vector<boost::uint8_t> dataReadBack(size);
            for (std::size_t i = 0; i < size; ++i) {
                data[i] = distRandomData(gen);
            }
            const unsigned int linkId = distLinkId(gen);
            const unsigned int priorityIndex = distPriorityIndex(gen);
            const abs_expiration_t absExpiration = distAbsExpiration(gen);

            BundleStorageManagerSession_WriteToDisk sessionWrite;
            bp_primary_if_base_t bundleMetaData;
            bundleMetaData.flags = (priorityIndex & 3) << 7;
            bundleMetaData.dst_node = DEST_LINKS[linkId];
            bundleMetaData.length = size;
            bundleMetaData.creation = 0;
            bundleMetaData.lifetime = absExpiration;
            //std::cout << "writing\n";
            boost::uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, bundleMetaData);
            //std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
            BOOST_REQUIRE_NE(totalSegmentsRequired, 0);

            for (boost::uint64_t i = 0; i < totalSegmentsRequired; ++i) {
                std::size_t bytesToCopy = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
                if (i == totalSegmentsRequired - 1) {
                    boost::uint64_t modBytes = (size % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
                    if (modBytes != 0) {
                        bytesToCopy = modBytes;
                    }
                }

                BOOST_REQUIRE(bsm.PushSegment(sessionWrite, &data[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE], bytesToCopy));
            }

            //std::cout << "reading\n";
            BundleStorageManagerSession_ReadFromDisk sessionRead;
            boost::uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
            //std::cout << "bytesToReadFromDisk " << bytesToReadFromDisk << "\n";
            BOOST_REQUIRE_EQUAL(bytesToReadFromDisk, size);

            //return top then take out again
            bsm.ReturnTop(sessionRead);
            bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
            //std::cout << "bytesToReadFromDisk after returned " << bytesToReadFromDisk << "\n";
            BOOST_REQUIRE_EQUAL(bytesToReadFromDisk, size);

            //check if custody was taken (should be empty)
            {
                BundleStorageManagerSession_ReadFromDisk sessionRead2;
                boost::uint64_t bytesToReadFromDisk2 = bsm.PopTop(sessionRead2, availableDestLinks);
                //std::cout << "bytesToReadFromDisk2 " << bytesToReadFromDisk2 << "\n";
                BOOST_REQUIRE_EQUAL(bytesToReadFromDisk2, 0);
            }

            BOOST_REQUIRE_MESSAGE(dataReadBack != data, "dataReadBack should not equal data yet");


            const std::size_t numSegmentsToRead = sessionRead.chainInfo.second.size();
            std::size_t totalBytesRead = 0;
            for (std::size_t i = 0; i < numSegmentsToRead; ++i) {
                totalBytesRead += bsm.TopSegment(sessionRead, &dataReadBack[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE]);
            }
            //std::cout << "totalBytesRead " << totalBytesRead << "\n";
            BOOST_REQUIRE_EQUAL(totalBytesRead, size);
            BOOST_REQUIRE_MESSAGE(dataReadBack == data, "dataReadBack does not equal data");
            BOOST_REQUIRE_MESSAGE(bsm.RemoveReadBundleFromDisk(sessionRead), "error freeing bundle from disk");

        }
    }
}








BOOST_AUTO_TEST_CASE(BundleStorageManagerAll_RestoreFromDisk_TestCase)
{
    for (unsigned int whichBsm = 0; whichBsm < 2; ++whichBsm) {
        boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
        const boost::random::uniform_int_distribution<> distRandomData(0, 255);
        const boost::random::uniform_int_distribution<> distPriorityIndex(0, 2);


        static const boost::uint64_t DEST_LINKS[10] = { 1,2,3,4,5,6,7,8,9,10 };
        std::vector<boost::uint64_t> availableDestLinks = { 1,2,3,4,5,6,7,8,9,10 };
        std::vector<boost::uint64_t> availableDestLinks2 = { 2 };




        static const boost::uint64_t sizes[15] = {
            BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
            BUNDLE_STORAGE_PER_SEGMENT_SIZE - 1 ,
            BUNDLE_STORAGE_PER_SEGMENT_SIZE - 0,
            BUNDLE_STORAGE_PER_SEGMENT_SIZE + 1,
            BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 1 ,
            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 0,
            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 1,
            2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 1 ,
            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 0,
            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 1,
            1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,
        };

        boost::uint64_t bytesWritten = 0, totalSegmentsWritten = 0;
        backup_memmanager_t backup;

        {
            std::unique_ptr<BundleStorageManagerBase> bsmPtr;
            StorageConfig_ptr ptrStorageConfig = StorageConfig::CreateFromJsonFile((Environment::GetPathHdtnSourceRoot() / "tests" / "config_files" / "storage" / "storageConfigRelativePaths.json").string());
            ptrStorageConfig->m_tryToRestoreFromDisk = false; //manually set this json entry
            ptrStorageConfig->m_autoDeleteFilesOnExit = false; //manually set this json entry
            if (whichBsm == 0) {
                std::cout << "create BundleStorageManagerMT for Restore" << std::endl;
                bsmPtr = boost::make_unique<BundleStorageManagerMT>(ptrStorageConfig);
            }
            else {
                std::cout << "create BundleStorageManagerAsio for Restore" << std::endl;
                bsmPtr = boost::make_unique<BundleStorageManagerAsio>(ptrStorageConfig);
            }
            BundleStorageManagerBase & bsm = *bsmPtr;

            bsm.Start();

            

            for (unsigned int sizeI = 0; sizeI < 15; ++sizeI) {
                const boost::uint64_t size = sizes[sizeI];
                //std::cout << "testing size " << size << "\n";
                std::vector<boost::uint8_t> data(size);

                for (std::size_t i = 0; i < size; ++i) {
                    data[i] = distRandomData(gen);
                }
                const unsigned int linkId = (sizeI == 12) ? 1 : 0;
                if (sizeI != 12) bytesWritten += size;
                const unsigned int priorityIndex = distPriorityIndex(gen);
                const abs_expiration_t absExpiration = sizeI;

                BundleStorageManagerSession_WriteToDisk sessionWrite;
                bp_primary_if_base_t bundleMetaData;
                bundleMetaData.flags = (priorityIndex & 3) << 7;
                bundleMetaData.dst_node = DEST_LINKS[linkId];
                bundleMetaData.length = size;
                bundleMetaData.creation = 0;
                bundleMetaData.lifetime = absExpiration;
                memcpy(&data[0], &bundleMetaData, sizeof(bundleMetaData));
                //std::cout << "writing\n";
                boost::uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, bundleMetaData);
                if (sizeI != 12) totalSegmentsWritten += totalSegmentsRequired;
                //std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
                BOOST_REQUIRE_NE(totalSegmentsRequired, 0);

                for (boost::uint64_t i = 0; i < totalSegmentsRequired; ++i) {
                    std::size_t bytesToCopy = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
                    if (i == totalSegmentsRequired - 1) {
                        boost::uint64_t modBytes = (size % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
                        if (modBytes != 0) {
                            bytesToCopy = modBytes;
                        }
                    }

                    BOOST_REQUIRE(bsm.PushSegment(sessionWrite, &data[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE], bytesToCopy));
                }
            }

            //delete a middle out
            BundleStorageManagerSession_ReadFromDisk sessionRead;
            boost::uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks2);
            BOOST_REQUIRE_EQUAL(bytesToReadFromDisk, sizes[12]);
            BOOST_REQUIRE_MESSAGE(bsm.RemoveReadBundleFromDisk(sessionRead, true), "error force freeing bundle from disk");

            bsm.GetMemoryManagerConstRef().BackupDataToVector(backup);
            BOOST_REQUIRE(bsm.GetMemoryManagerConstRef().IsBackupEqual(backup));
        }

        std::cout << "wrote bundles but leaving files\n";
        //boost::this_thread::sleep(boost::posix_time::milliseconds(500));
        std::cout << "restoring...\n";
        {
            std::unique_ptr<BundleStorageManagerBase> bsmPtr;
            StorageConfig_ptr ptrStorageConfig = StorageConfig::CreateFromJsonFile((Environment::GetPathHdtnSourceRoot() / "tests" / "config_files" / "storage" / "storageConfigRelativePaths.json").string());
            ptrStorageConfig->m_tryToRestoreFromDisk = true; //manually set this json entry
            ptrStorageConfig->m_autoDeleteFilesOnExit = true; //manually set this json entry
            if (whichBsm == 0) {
                std::cout << "create BundleStorageManagerMT for Restore" << std::endl;
                bsmPtr = boost::make_unique<BundleStorageManagerMT>(ptrStorageConfig);
            }
            else {
                std::cout << "create BundleStorageManagerAsio for Restore" << std::endl;
                bsmPtr = boost::make_unique<BundleStorageManagerAsio>(ptrStorageConfig);
            }
            BundleStorageManagerBase & bsm = *bsmPtr;


            
            //BOOST_REQUIRE(!bsm.GetMemoryManagerConstRef().IsBackupEqual(backup));
            BOOST_REQUIRE_MESSAGE(bsm.m_successfullyRestoredFromDisk, "error restoring from disk");
            BOOST_REQUIRE(bsm.GetMemoryManagerConstRef().IsBackupEqual(backup));
            std::cout << "restored\n";
            BOOST_REQUIRE_EQUAL(bsm.m_totalBundlesRestored, (15 - 1));
            BOOST_REQUIRE_EQUAL(bsm.m_totalBytesRestored, bytesWritten);
            BOOST_REQUIRE_EQUAL(bsm.m_totalSegmentsRestored, totalSegmentsWritten);

            bsm.Start();




            uint64_t totalBytesReadFromRestored = 0, totalSegmentsReadFromRestored = 0;
            for (unsigned int sizeI = 0; sizeI < (15 - 1); ++sizeI) {


                //std::cout << "reading\n";
                BundleStorageManagerSession_ReadFromDisk sessionRead;
                boost::uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
                //std::cout << "bytesToReadFromDisk " << bytesToReadFromDisk << "\n";
                BOOST_REQUIRE_NE(bytesToReadFromDisk, 0);
                std::vector<boost::uint8_t> dataReadBack(bytesToReadFromDisk);
                totalBytesReadFromRestored += bytesToReadFromDisk;

                const std::size_t numSegmentsToRead = sessionRead.chainInfo.second.size();
                totalSegmentsReadFromRestored += numSegmentsToRead;
                std::size_t totalBytesRead = 0;
                for (std::size_t i = 0; i < numSegmentsToRead; ++i) {
                    totalBytesRead += bsm.TopSegment(sessionRead, &dataReadBack[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE]);
                }
                //std::cout << "totalBytesRead " << totalBytesRead << "\n";
                BOOST_REQUIRE_EQUAL(totalBytesRead, bytesToReadFromDisk);

                BOOST_REQUIRE_MESSAGE(bsm.RemoveReadBundleFromDisk(sessionRead), "error freeing bundle from disk");

            }

            BOOST_REQUIRE_EQUAL(totalBytesReadFromRestored, bytesWritten);
            BOOST_REQUIRE_EQUAL(totalSegmentsReadFromRestored, totalSegmentsWritten);



        }
    }
}
