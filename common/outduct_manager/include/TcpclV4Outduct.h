#ifndef TCPCLV4_OUTDUCT_H
#define TCPCLV4_OUTDUCT_H 1

#include <string>
#include "Outduct.h"
#include "TcpclV4BundleSource.h"
#include <list>

class TcpclV4Outduct : public Outduct {
public:
    TcpclV4Outduct(const outduct_element_config_t & outductConfig, const uint64_t myNodeId, const uint64_t outductUuid,
        const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback = OutductOpportunisticProcessReceivedBundleCallback_t());
    virtual ~TcpclV4Outduct();
    virtual std::size_t GetTotalDataSegmentsUnacked();
    virtual bool Forward(const uint8_t* bundleData, const std::size_t size);
    virtual bool Forward(zmq::message_t & movableDataZmq);
    virtual bool Forward(std::vector<uint8_t> & movableDataVec);
    virtual void SetOnSuccessfulAckCallback(const OnSuccessfulOutductAckCallback_t & callback);
    virtual void Connect();
    virtual bool ReadyToForward();
    virtual void Stop();
    virtual void GetOutductFinalStats(OutductFinalStats & finalStats);

private:
    TcpclV4Outduct();


    TcpclV4BundleSource m_tcpclV4BundleSource;
};


#endif // TCPCLV4_OUTDUCT_H

