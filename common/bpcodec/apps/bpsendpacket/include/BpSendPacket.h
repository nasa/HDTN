/**
 * @file BpSendFile.h
 * @author  Timothy Recker <tjr@berkeley.edu>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * The BpSendPacket class is a child class of BpSourcePattern.  It is an app 
 * used for extracting payload data from a (UDP) packet, wrapping it into a 
 * bundle, and sending it. It is episodic and overrides 
 * TryWaitForDataAvailable since it monitors a socket that will not always
 *  have new data.
 */

#ifndef _BP_SEND_PACKET_H
#define _BP_SEND_PACKET_H 1

#include <cstdint>
#include <queue>
#include <boost/function.hpp>
#include "app_patterns/BpSourcePattern.h"


typedef struct Payload {
    uint64_t length;
    uint8_t *data;
} payload_t;

typedef boost::function<void(padded_vector_uint8_t & packet)> ProcessPacketCallback_t;

class BpSendPacket : public BpSourcePattern {
// private:
    
public:
    BpSendPacket();
    BpSendPacket(std::string host);
    bool Init(InductsConfig_ptr & inductsConfigPtr, const cbhe_eid_t & myEid, const uint64_t maxBundleSizeBytes);
    virtual ~BpSendPacket() override;
protected:
    virtual bool TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) override;
    virtual uint64_t GetNextPayloadLength_Step1() override;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) override;
private:
    void ProcessPacketCallback(padded_vector_uint8_t & packet);
    void NullCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr);
    const ProcessPacketCallback_t m_processPacketCallback;
    InductManager m_packetInductManager;
    // m_inductPtr std::unique_ptr<Induct>;
    // UdpBundleSink m_udpSink;
    std::queue<padded_vector_uint8_t> m_queue;
    std::string m_host;
    // uint16_t m_port;
};
#endif //_BP_SEND_PACKET_H
    
