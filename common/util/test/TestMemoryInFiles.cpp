/**
 * @file TestMemoryInFiles.cpp
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
#include "MemoryInFiles.h"
#include <boost/bind/bind.hpp>


BOOST_AUTO_TEST_CASE(MemoryInFilesTestCase)
{
    namespace fs = boost::filesystem;

    struct Test {
        boost::asio::io_service ioService;
        const fs::path rootPath;
        const uint64_t newFileAggregationTimeMs;
        MemoryInFiles mf;
        std::shared_ptr<std::string> lastDataReadBackCallbackSharedPtr;
        Test(const fs::path & paramRootPath) :
            rootPath(paramRootPath),
            newFileAggregationTimeMs(10000),
            mf(ioService, rootPath, newFileAggregationTimeMs) {}

        void DoTest() {
            uint64_t memoryBlockId;
            {
                const std::size_t totalMemoryBlockSize = 10;
                memoryBlockId = mf.AllocateNewWriteMemoryBlock(totalMemoryBlockSize);
                BOOST_REQUIRE_EQUAL(memoryBlockId, 1);
                std::shared_ptr<std::string> dataSharedPtr = std::make_shared<std::string>("56789");
                BOOST_REQUIRE_EQUAL(dataSharedPtr->size(), 5);
                const char* data = dataSharedPtr->data();
                const std::size_t sizeData = dataSharedPtr->size();
                BOOST_REQUIRE(mf.WriteMemoryAsync(memoryBlockId, 5, data, sizeData, boost::bind(&Test::WriteHandler, this, std::move(dataSharedPtr))));
                BOOST_REQUIRE(!dataSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run(), 1);
                ioService.reset();
            }
            { //read only "678"
                lastDataReadBackCallbackSharedPtr.reset();
                std::shared_ptr<std::string> dataReadBackSharedPtr = std::make_shared<std::string>("aaa");
                char* data = dataReadBackSharedPtr->data();
                const std::size_t sizeData = dataReadBackSharedPtr->size();
                BOOST_REQUIRE(mf.ReadMemoryAsync(memoryBlockId, 6, data, sizeData, boost::bind(&Test::ReadHandler,
                    this, boost::placeholders::_1, std::move(dataReadBackSharedPtr))));
                BOOST_REQUIRE(!dataReadBackSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE(lastDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL(*lastDataReadBackCallbackSharedPtr, "678");
            }
        }

        void ReadHandler(bool success, std::shared_ptr<std::string>& dataSharedPtr) {
            BOOST_REQUIRE(dataSharedPtr);
            lastDataReadBackCallbackSharedPtr = std::move(dataSharedPtr);
        }
        void WriteHandler(std::shared_ptr<std::string> & dataSharedPtr) {
            BOOST_REQUIRE(dataSharedPtr);
        }
    };
    
    const fs::path rootPath = fs::temp_directory_path() / "MemoryInFilesTest";
    if (boost::filesystem::is_directory(rootPath)) {
        fs::remove_all(rootPath);
    }
    BOOST_REQUIRE(boost::filesystem::create_directory(rootPath));
    std::cout << "running MemoryInFilesTestCase with rootpath=" << rootPath << "\n";
        
    Test t(rootPath);
    t.DoTest();
}
