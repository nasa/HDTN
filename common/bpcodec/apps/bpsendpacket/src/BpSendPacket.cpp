#include <iostream>
// #include "UdpInduct.h"
#include "app_patterns/BpSinkPattern.h"
#include "BpSendPacket.h"

// BpSendPacket::BpSendPacket() :
//     BpSendPacket("localhost", 7132)
// {
// }

BpSendPacket::BpSendPacket()
    : BpSourcePattern()
{
}

bool BpSendPacket::Init(InductsConfig_ptr & inductsConfigPtr, const cbhe_eid_t & myEid, const uint64_t maxBundleSizeBytes) {
    m_packetInductManager.LoadInductsFromConfig(
        boost::bind(&BpSendPacket::ProcessPacketCallback, this, boost::placeholders::_1),
        *inductsConfigPtr, myEid.nodeId, UINT16_MAX, maxBundleSizeBytes,
        boost::bind(&BpSendPacket::NullCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&BpSendPacket::NullCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    return true;
}

void BpSendPacket::NullCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr) {
    return;
}

BpSendPacket::~BpSendPacket() {}

uint64_t BpSendPacket::GetNextPayloadLength_Step1() {
    if (m_queue.empty()) {
        return UINT64_MAX;
    } else {
        uint64_t len = m_queue.front().size();
        // std::cout << "got size " << len << std::endl;
        return len;
    }
}

bool BpSendPacket::CopyPayload_Step2(uint8_t * destinationBuffer) {
    memcpy(destinationBuffer, &m_queue.front(), m_queue.front().size());
    m_queue.pop();
    // std::cout << "copied payload" << std::endl;
    return true;
}

bool BpSendPacket::TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) {
    if (m_queue.empty()) {
        // std::cout << "waiting..." << std::endl;
        boost::this_thread::sleep(timeout);
        return false;
    }
    return true;
}

void BpSendPacket::ProcessPacketCallback(padded_vector_uint8_t & packet) {
    m_queue.emplace(packet);
}
