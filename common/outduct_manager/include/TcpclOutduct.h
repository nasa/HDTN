#ifndef TCPCL_OUTDUCT_H
#define TCPCL_OUTDUCT_H 1

#include <string>
#include "Outduct.h"
#include "TcpclBundleSource.h"
#include <list>

class TcpclOutduct : public Outduct {
public:
    TcpclOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    virtual ~TcpclOutduct();
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
    TcpclOutduct();


    TcpclBundleSource m_tcpclBundleSource;
};


#endif // TCPCL_OUTDUCT_H

