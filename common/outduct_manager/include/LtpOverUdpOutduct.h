#ifndef LTP_OVER_UDP_OUTDUCT_H
#define LTP_OVER_UDP_OUTDUCT_H 1

#include <string>
#include "Outduct.h"
#include "LtpBundleSource.h"
#include <list>

class LtpOverUdpOutduct : public Outduct {
public:
    LtpOverUdpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    virtual ~LtpOverUdpOutduct();
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
    LtpOverUdpOutduct();


    LtpBundleSource m_ltpBundleSource;
};


#endif // STCP_OUTDUCT_H

