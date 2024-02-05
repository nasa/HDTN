#include "BpReceivePacket.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BpReceivePacket::BpReceivePacket()
    : BpSinkPattern()
{
}

BpReceivePacket::~BpReceivePacket() {}

bool BpReceivePacket::ProcessPayload(const uint8_t * data, const uint64_t size) {
    std::vector<uint8_t> userdata(sizeof(bundleid_payloadsize_pair_t));
    Outduct* outduct = m_packetOutductManager.GetOutductByOutductUuid(0);
    
    if (!outduct) {
        LOG_ERROR(subprocess) << "null outduct";
        return false;
    }

    if (!outduct->Forward(data, size, std::move(userdata))) {
        LOG_ERROR(subprocess) << "[Receive app] unable to send bundle on the outduct.";
        return false;
    }
    else {
        LOG_DEBUG(subprocess) << "[ReceivePacket app] Transferred bundle to UDP";
    }
    return true;
}

bool BpReceivePacket::socketInit(OutductsConfig_ptr & outductsConfigPtr, const cbhe_eid_t & myEid, const uint64_t maxBundleSizeBytes) {
    (void)maxBundleSizeBytes;
    LOG_DEBUG(subprocess) << "[ReceivePacket app] INIT";
    if (!m_packetOutductManager.LoadOutductsFromConfig(*outductsConfigPtr, myEid.nodeId, UINT16_MAX, 10000000))
    {
        return false;
    }
    LOG_DEBUG(subprocess) << "[ReceivePacket app] INITIALIZED PACKET OUTDUCT";
    return true;
}
