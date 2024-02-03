#include <iostream>
#include "app_patterns/BpSinkPattern.h"
#include "BpSendPacket.h"

BpSendPacket::BpSendPacket(const uint64_t maxBundleSizeBytes)
    : BpSourcePattern(),
      m_maxBundleSizeBytes(maxBundleSizeBytes)
{
}

bool BpSendPacket::Init(InductsConfig_ptr & inductsConfigPtr, const cbhe_eid_t & myEid) {
    if (inductsConfigPtr) {
          m_packetInductManager.LoadInductsFromConfig(
        boost::bind(&BpSendPacket::ProcessPacketCallback, this, boost::placeholders::_1),
        *inductsConfigPtr, myEid.nodeId, UINT16_MAX, m_maxBundleSizeBytes,
        boost::bind(&BpSendPacket::NullCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&BpSendPacket::NullCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3)); 
          return true;
    }
    return false;
}

void BpSendPacket::NullCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr) {
    (void)remoteNodeId;
    (void)thisInductPtr;
    (void)sinkPtr;
    return;
}

BpSendPacket::~BpSendPacket() {}

uint64_t BpSendPacket::GetNextPayloadLength_Step1() {
    if (m_queue.empty()) {
        return UINT64_MAX;
    } else {
        uint64_t len = m_queue.front().size();
        return len;
    }
}

bool BpSendPacket::CopyPayload_Step2(uint8_t * destinationBuffer) {
    std::size_t len = m_queue.front().size();
    padded_vector_uint8_t data = m_queue.front();

    std::copy(data.begin(), data.end(), destinationBuffer);
    m_queue.pop();
    
    std::cout <<"[Send app] ";
    for (std::size_t i = 0; i < len; i++) {
        std::cout << destinationBuffer[i];
    }
    std::cout << " (" << len << " bytes)" << std::endl;
    
    return true;
}

bool BpSendPacket::TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) {
    if (m_queue.empty()) {
        boost::this_thread::sleep(timeout);
        return false;
    }
    return true;
}

void BpSendPacket::ProcessPacketCallback(padded_vector_uint8_t & packet) {
    m_queue.emplace(packet);
}
