#ifndef STCP_OUTDUCT_H
#define STCP_OUTDUCT_H 1

#include <string>
#include "Outduct.h"
#include "StcpBundleSource.h"
#include <list>

class CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB StcpOutduct : public Outduct {
public:
    OUTDUCT_MANAGER_LIB_EXPORT StcpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    OUTDUCT_MANAGER_LIB_EXPORT virtual ~StcpOutduct() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual std::size_t GetTotalDataSegmentsUnacked() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(std::vector<uint8_t> & movableDataVec, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetUserAssignedUuid(uint64_t userAssignedUuid) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void Connect() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool ReadyToForward() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void Stop() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void GetOutductFinalStats(OutductFinalStats & finalStats) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual uint64_t GetOutductTelemetry(uint8_t* data, uint64_t bufferSize) override;
    
private:
    StcpOutduct();
    

    StcpBundleSource m_stcpBundleSource;
};


#endif // STCP_OUTDUCT_H

