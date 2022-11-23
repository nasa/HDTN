/**
 * @file TestTelemetry.cpp
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
#include "Telemetry.h"


BOOST_AUTO_TEST_CASE(TelemetryTestCase)
{
    
    {
        OutductCapabilityTelemetry_t oct;
        
        BOOST_REQUIRE_EQUAL(oct.type, 5);
        oct.maxBundlesInPipeline = 50;
        oct.maxBundleSizeBytesInPipeline = 5000;
        oct.outductArrayIndex = 2;
        oct.nextHopNodeId = 10;
        oct.finalDestinationEidList = { cbhe_eid_t(1,1), cbhe_eid_t(2,1) };
        oct.finalDestinationNodeIdList = { 3, 4, 5 };

        const uint64_t serializationSize = oct.GetSerializationSize();
        BOOST_REQUIRE_EQUAL(serializationSize, (7*8) + (2*16) + (3*8));

        std::vector<uint8_t> serialized(serializationSize);
        BOOST_REQUIRE_EQUAL(oct.SerializeToLittleEndian(serialized.data(), serializationSize - 1), 0); //fail due to too small of buffer size
        BOOST_REQUIRE_EQUAL(oct.SerializeToLittleEndian(serialized.data(), serializationSize), serializationSize);
        OutductCapabilityTelemetry_t oct2;
        uint64_t numBytesTakenToDecode;
        BOOST_REQUIRE(!oct2.DeserializeFromLittleEndian(serialized.data(), numBytesTakenToDecode, serializationSize - 1)); //fail due to too small of buffer size
        BOOST_REQUIRE(oct2.DeserializeFromLittleEndian(serialized.data(), numBytesTakenToDecode, serializationSize));
        
        BOOST_REQUIRE(oct == oct2);
        
        //misc
        BOOST_REQUIRE(!(oct != oct2));
        OutductCapabilityTelemetry_t octCopy = oct;
        OutductCapabilityTelemetry_t octCopy2(oct);
        OutductCapabilityTelemetry_t oct2Moved = std::move(oct2);
        BOOST_REQUIRE(oct != oct2); //oct2 moved
        BOOST_REQUIRE(oct == oct2Moved);
        BOOST_REQUIRE(oct == octCopy);
        BOOST_REQUIRE(oct == octCopy2);
        OutductCapabilityTelemetry_t oct2Moved2(std::move(oct2Moved));
        BOOST_REQUIRE(oct != oct2Moved); //oct2 moved
        BOOST_REQUIRE(oct == oct2Moved2);
    }

    {
        AllOutductCapabilitiesTelemetry_t aoct;

        BOOST_REQUIRE_EQUAL(aoct.type, 6);
        uint64_t expectedSerializationSize = 2 * sizeof(uint64_t);
        for (unsigned int i = 0; i < 10; ++i) {
            OutductCapabilityTelemetry_t& oct = aoct.outductCapabilityTelemetryList.emplace_back();
            oct.maxBundlesInPipeline = 50 + i;
            oct.maxBundleSizeBytesInPipeline = 5000 + i;
            oct.outductArrayIndex = i;
            oct.nextHopNodeId = 10 + i;
            const unsigned int base = i * 100;
            oct.finalDestinationEidList = { cbhe_eid_t(base + 1,1), cbhe_eid_t(base + 2,1) };
            oct.finalDestinationNodeIdList = { base + 3, base + 4, base + 5 };

            const uint64_t serializationSize = oct.GetSerializationSize();
            BOOST_REQUIRE_EQUAL(serializationSize, (7 * 8) + (2 * 16) + (3 * 8));
            expectedSerializationSize += serializationSize;
        }

        const uint64_t serializationSize = aoct.GetSerializationSize();
        BOOST_REQUIRE_EQUAL(serializationSize, expectedSerializationSize);

        std::vector<uint8_t> serialized(serializationSize);
        BOOST_REQUIRE_EQUAL(aoct.SerializeToLittleEndian(serialized.data(), serializationSize - 1), 0); //fail due to too small of buffer size
        BOOST_REQUIRE_EQUAL(aoct.SerializeToLittleEndian(serialized.data(), serializationSize), serializationSize);
        AllOutductCapabilitiesTelemetry_t aoct2;
        uint64_t numBytesTakenToDecode;
        BOOST_REQUIRE(!aoct2.DeserializeFromLittleEndian(serialized.data(), numBytesTakenToDecode, serializationSize - 1)); //fail due to too small of buffer size
        BOOST_REQUIRE(aoct2.DeserializeFromLittleEndian(serialized.data(), numBytesTakenToDecode, serializationSize));

        BOOST_REQUIRE(aoct == aoct2);
        //std::cout << aoct2 << std::endl;

        //misc
        BOOST_REQUIRE(!(aoct != aoct2));
        AllOutductCapabilitiesTelemetry_t aoctCopy = aoct;
        AllOutductCapabilitiesTelemetry_t aoctCopy2(aoct);
        AllOutductCapabilitiesTelemetry_t aoct2Moved = std::move(aoct2);
        BOOST_REQUIRE(aoct != aoct2); //oct2 moved
        BOOST_REQUIRE(aoct == aoct2Moved);
        BOOST_REQUIRE(aoct == aoctCopy);
        BOOST_REQUIRE(aoct == aoctCopy2);
        AllOutductCapabilitiesTelemetry_t aoct2Moved2(std::move(aoct2Moved));
        BOOST_REQUIRE(aoct != aoct2Moved); //oct2 moved
        BOOST_REQUIRE(aoct == aoct2Moved2);
    }

}
