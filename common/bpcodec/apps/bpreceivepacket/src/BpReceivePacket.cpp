/**
 * @file BpReceiveFile.cpp
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

#include "BpReceivePacket.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/filesystem.hpp>
#include "Utf8Paths.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BpReceivePacket::BpReceivePacket()
    : BpSinkPattern()
{
}

BpReceivePacket::~BpReceivePacket() {}

bool BpReceivePacket::ProcessPayload(const uint8_t * data, const uint64_t size) {
    std::cout << "RECEIVED BUNDLE" << std::endl;
    // LOG_INFO(subprocess) << "Received bundle";
    std::vector<uint8_t> userdata(sizeof(bundleid_payloadsize_pair_t));
    // bundleid_payloadsize_pair_t* bundleToSendUserDataPairPtr = (bundleid_payloadsize_pair_t*)bundleToSendUserData.data();
    // std::vector<uint8_t> bundleToSend;
    Outduct* outduct = m_packetOutductManager.GetOutductByOutductUuid(0);
    // uint64_t outductMaxBundlesInPipeline = 0;
    
    if (outduct) {
        // if (outduct != m_packetOutductManager.GetOutductByFinalDestinationEid_ThreadSafe(m_finalDestinationEid)) {
        //     LOG_ERROR(subprocess) << "outduct 0 does not support finalDestinationEid " << m_finalDestinationEid;
        //     return false;
        // }
    }
    else {
        LOG_ERROR(subprocess) << "null outduct";
        return false;
    }

    if (!outduct->Forward(data, size, std::move(userdata))) {
        LOG_ERROR(subprocess) << "BpSourcePattern unable to send bundle on the outduct.. retrying in 1 second";
        return false;
        // boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
    else {
        LOG_INFO(subprocess) << "Transferred bundle to UDP";
        // std::cout << std::string(data, size) << std::endl;
        std::cout << data << std::endl;
    }
    return true;
}

bool BpReceivePacket::socketInit(OutductsConfig_ptr & outductsConfigPtr, const cbhe_eid_t & myEid, const uint64_t maxBundleSizeBytes) {
    std::cout << "INIT" << std::endl;
    if (!m_packetOutductManager.LoadOutductsFromConfig(*outductsConfigPtr, myEid.nodeId, UINT16_MAX, 10000000))
    {
        return false;
    }
    std::cout << "INITIALIZED PACKET OUTDUCT" << std::endl;
    return true;
}
