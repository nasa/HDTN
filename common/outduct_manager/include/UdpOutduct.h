#ifndef UDP_OUTDUCT_H
#define UDP_OUTDUCT_H 1

#include <string>
#include "Outduct.h"
#include "UdpBundleSource.h"
#include <list>

class UdpOutduct : public Outduct {
public:
    UdpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    virtual ~UdpOutduct();
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
    UdpOutduct();


    UdpBundleSource m_udpBundleSource;
};


#endif // UDP_OUTDUCT_H

