#include <boost/test/unit_test.hpp>
#include "BundleStorageManagerMT.h"
#include <iostream>
#include <string>
#include <boost/filesystem.hpp>

#define BUNDLES_TO_SEND 10

BOOST_AUTO_TEST_CASE(BundleStorageManagerMtAsFifoTestCase)
{

	BundleStorageManagerMT bsm("storageConfigRelativePaths.json");
	bsm.Start();


	static const uint64_t DEST_LINKS[BUNDLES_TO_SEND] = { 1,2,3,4,2,3,4,1,2,1 };
	static const uint64_t BUNDLE_SIZES[BUNDLES_TO_SEND] = { 10000000,11000000,12000000,13000000,14000000,15000000,16000000,17000000,18000000,19000000 }; //10MB to 19MB bundles

	//SEND 10 BUNDLES
	for (unsigned int bundleI = 0; bundleI < BUNDLES_TO_SEND; ++bundleI) {

		const boost::uint64_t bundleSize = BUNDLE_SIZES[bundleI];
		//std::cout << "testing size " << size << "\n";
		std::vector<boost::uint8_t> bundleToWrite(bundleSize, 0); //filled with zeros
		
		
		
		const uint64_t linkId = DEST_LINKS[bundleI];
		const unsigned int priorityIndex = 0; //could be 0, 1, or 2, but keep 0 for fifo mode
		const abs_expiration_t absExpiration = bundleI; //increment this every time for fifo mode
														
		//set a value at the 5MB position at the bundle	for read back verification
		memcpy(&bundleToWrite[5000000], &linkId, sizeof(uint64_t));

		BundleStorageManagerSession_WriteToDisk sessionWrite;
		bp_primary_if_base_t bundleMetaData;
		bundleMetaData.flags = (priorityIndex & 3) << 7;
		bundleMetaData.dst_node = linkId;
		bundleMetaData.length = bundleSize;
		bundleMetaData.creation = 0;
		bundleMetaData.lifetime = absExpiration;
		//std::cout << "writing\n";
		boost::uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, bundleMetaData);
		//std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
		BOOST_REQUIRE_NE(totalSegmentsRequired, 0); //if total segments reqired is 0 then there was an error

		for (boost::uint64_t i = 0; i < totalSegmentsRequired; ++i) {
			std::size_t bytesToCopy = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
			if (i == totalSegmentsRequired - 1) { //if this is the last segment in the bundle, calculate only the remaining bytes needed to copy
				boost::uint64_t modBytes = (bundleSize % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
				if (modBytes != 0) {
					bytesToCopy = modBytes;
				}
			}
			int successPushSegment = bsm.PushSegment(sessionWrite, &bundleToWrite[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE], bytesToCopy);
			BOOST_REQUIRE(successPushSegment);
		}
	}

	//READ BACK THE 10 BUNDLES
	unsigned int numBundlesReadBack = 0; //counter to make sure we read back 10 bundles
	for (unsigned int linkToRead = 1; linkToRead <= 4; ++linkToRead) {
		boost::uint64_t previousBundleSize = 0;  //value to verify fifo working since we kept writing bigger bundles sizes
		while(1) {

			const boost::uint64_t largestBundleSize = 20000000; //20MB is larger than our bundles
			std::vector<boost::uint64_t> availableDestLinks = { linkToRead }; //fifo mode, only put in the one link you want to read (could be more tho)

			std::vector<boost::uint8_t> bundleReadBack(largestBundleSize, 0); //initialize it to zero because why not
			//std::cout << "reading\n";
			BundleStorageManagerSession_ReadFromDisk sessionRead;
			boost::uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
			//std::cout << "bytesToReadFromDisk " << bytesToReadFromDisk << "\n";
			if (bytesToReadFromDisk == 0) { //no more of these links to read
				break; 
			}
			else {//this link has a bundle in the fifo
				BOOST_REQUIRE_GT(bytesToReadFromDisk, previousBundleSize);
				previousBundleSize = bytesToReadFromDisk;

				//USE THIS COMMENTED OUT CODE IF YOU DECIDE YOU DON'T WANT TO READ THE BUNDLE AFTER PEEKING AT IT (MAYBE IT'S TOO BIG RIGHT NOW)
				//return top then take out again
				//bsm.ReturnTop(sessionRead);
				//bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks); //get it back
			
			
				const std::size_t numSegmentsToRead = sessionRead.chainInfo.second.size();
				std::size_t totalBytesRead = 0;
				for (std::size_t i = 0; i < numSegmentsToRead; ++i) {
					totalBytesRead += bsm.TopSegment(sessionRead, &bundleReadBack[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE]);
				}
				//std::cout << "totalBytesRead " << totalBytesRead << "\n";
				BOOST_REQUIRE_EQUAL(totalBytesRead, bytesToReadFromDisk);

				uint64_t linkIdReadBack;
				memcpy(&linkIdReadBack, &bundleReadBack[5000000], sizeof(uint64_t));
				BOOST_REQUIRE_EQUAL(linkIdReadBack, linkToRead); //verify that 5MB position

				//if you're happy with the bundle data you read back, then officially remove it from the disk
				bool successRemoveBundle = bsm.RemoveReadBundleFromDisk(sessionRead);
				BOOST_REQUIRE_MESSAGE(successRemoveBundle, "error freeing bundle from disk");
				++numBundlesReadBack;
			}
			

		}
	}
	BOOST_REQUIRE_EQUAL(numBundlesReadBack, BUNDLES_TO_SEND);
}




BOOST_AUTO_TEST_CASE(BundleStorageManagerMtAsFifo_RestoreFromDisk_TestCase)
{

	static const uint64_t DEST_LINKS[BUNDLES_TO_SEND] = { 1,2,3,4,2,3,4,1,2,1 };
	static const uint64_t BUNDLE_SIZES[BUNDLES_TO_SEND] = { 10000000,11000000,12000000,13000000,14000000,15000000,16000000,17000000,18000000,19000000 }; //10MB to 19MB bundles
	
	{ //scope this bsm instance (deleted when going out of scope)
        BundleStorageManagerMT bsm("storageConfigRelativePaths.json");
		bsm.Start(false); //false => disable autodelete files on exit
		//SEND 10 BUNDLES
		for (unsigned int bundleI = 0; bundleI < BUNDLES_TO_SEND; ++bundleI) {

			const boost::uint64_t bundleSize = BUNDLE_SIZES[bundleI];
			//std::cout << "testing size " << size << "\n";
			std::vector<boost::uint8_t> bundleToWrite(bundleSize, 0); //filled with zeros



			const uint64_t linkId = DEST_LINKS[bundleI];
			const unsigned int priorityIndex = 0; //could be 0, 1, or 2, but keep 0 for fifo mode
			const abs_expiration_t absExpiration = bundleI; //increment this every time for fifo mode

			//set a value at the 5MB position at the bundle	for read back verification
			memcpy(&bundleToWrite[5000000], &linkId, sizeof(uint64_t));

			BundleStorageManagerSession_WriteToDisk sessionWrite;
			bp_primary_if_base_t bundleMetaData;
			bundleMetaData.flags = (priorityIndex & 3) << 7;
			bundleMetaData.dst_node = linkId;
			bundleMetaData.length = bundleSize;
			bundleMetaData.creation = 0;
			bundleMetaData.lifetime = absExpiration;

			//TODO.. CURRENTLY THIS MEMCPY IS REQIRED FOR RESTORE FROM DISK TO WORK... WILL NEED TO EDIT THE RESTORE FUNCTION TO SUPPORT PROPER BUNDLE HEADERS
			memcpy(&bundleToWrite[0], &bundleMetaData, sizeof(bundleMetaData));


			//std::cout << "writing\n";
			boost::uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, bundleMetaData);
			//std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
			BOOST_REQUIRE_NE(totalSegmentsRequired, 0); //if total segments reqired is 0 then there was an error

			for (boost::uint64_t i = 0; i < totalSegmentsRequired; ++i) {
				std::size_t bytesToCopy = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
				if (i == totalSegmentsRequired - 1) { //if this is the last segment in the bundle, calculate only the remaining bytes needed to copy
					boost::uint64_t modBytes = (bundleSize % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
					if (modBytes != 0) {
						bytesToCopy = modBytes;
					}
				}
				int successPushSegment = bsm.PushSegment(sessionWrite, &bundleToWrite[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE], bytesToCopy);
				BOOST_REQUIRE(successPushSegment);
			}
		}
		boost::this_thread::sleep(boost::posix_time::seconds(3)); //give the threads time to complete any writes
	}

	std::cout << "wrote bundles but leaving files (fifo)\n";	
	std::cout << "restoring (fifo)...\n";

	{
        BundleStorageManagerMT bsm("storageConfigRelativePaths.json");
		uint64_t totalBundlesRestored, totalBytesRestored, totalSegmentsRestored;
		BOOST_REQUIRE_MESSAGE(bsm.RestoreFromDisk(&totalBundlesRestored, &totalBytesRestored, &totalSegmentsRestored), "error restoring from disk");
		
		std::cout << "restored (fifo)\n";
		BOOST_REQUIRE_EQUAL(totalBundlesRestored, BUNDLES_TO_SEND);
		bsm.Start();

		//READ BACK THE 10 BUNDLES
		unsigned int numBundlesReadBack = 0; //counter to make sure we read back 10 bundles
		for (unsigned int linkToRead = 1; linkToRead <= 4; ++linkToRead) {
			boost::uint64_t previousBundleSize = 0;  //value to verify fifo working since we kept writing bigger bundles sizes
			while (1) {

				const boost::uint64_t largestBundleSize = 20000000; //20MB is larger than our bundles
				std::vector<boost::uint64_t> availableDestLinks = { linkToRead }; //fifo mode, only put in the one link you want to read (could be more tho)

				std::vector<boost::uint8_t> bundleReadBack(largestBundleSize, 0); //initialize it to zero because why not
				//std::cout << "reading\n";
				BundleStorageManagerSession_ReadFromDisk sessionRead;
				boost::uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
				//std::cout << "bytesToReadFromDisk " << bytesToReadFromDisk << "\n";
				if (bytesToReadFromDisk == 0) { //no more of these links to read
					break;
				}
				else {//this link has a bundle in the fifo
					BOOST_REQUIRE_GT(bytesToReadFromDisk, previousBundleSize);
					previousBundleSize = bytesToReadFromDisk;
					//USE THIS COMMENTED OUT CODE IF YOU DECIDE YOU DON'T WANT TO READ THE BUNDLE AFTER PEEKING AT IT (MAYBE IT'S TOO BIG RIGHT NOW)
					//return top then take out again
					//bsm.ReturnTop(sessionRead);
					//bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks); //get it back


					const std::size_t numSegmentsToRead = sessionRead.chainInfo.second.size();
					std::size_t totalBytesRead = 0;
					for (std::size_t i = 0; i < numSegmentsToRead; ++i) {
						totalBytesRead += bsm.TopSegment(sessionRead, &bundleReadBack[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE]);
					}
					//std::cout << "totalBytesRead " << totalBytesRead << "\n";
					BOOST_REQUIRE_EQUAL(totalBytesRead, bytesToReadFromDisk);

					uint64_t linkIdReadBack;
					memcpy(&linkIdReadBack, &bundleReadBack[5000000], sizeof(uint64_t));
					BOOST_REQUIRE_EQUAL(linkIdReadBack, linkToRead); //verify that 5MB position

					//if you're happy with the bundle data you read back, then officially remove it from the disk
					bool successRemoveBundle = bsm.RemoveReadBundleFromDisk(sessionRead);
					BOOST_REQUIRE_MESSAGE(successRemoveBundle, "error freeing bundle from disk");
					++numBundlesReadBack;
				}


			}
		}
		BOOST_REQUIRE_EQUAL(numBundlesReadBack, BUNDLES_TO_SEND);
	}
}

