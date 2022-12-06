#ifndef OUTDUCT_H
#define OUTDUCT_H 1
#include "outduct_manager_lib_export.h"
#ifndef CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB
#  else
#    define CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB OUTDUCT_MANAGER_LIB_EXPORT
#  endif
#endif
#include <string>
#include <boost/integer.hpp>
#include <boost/function.hpp>
#include "OutductsConfig.h"
#include <list>
#include <zmq.hpp>
#include "BundleCallbackFunctionDefines.h"

struct OutductFinalStats {
    std::string m_convergenceLayer;
    std::size_t m_totalDataSegmentsOrPacketsSent;
    std::size_t m_totalDataSegmentsOrPacketsAcked;

    OutductFinalStats() : m_convergenceLayer(""), m_totalDataSegmentsOrPacketsSent(0), m_totalDataSegmentsOrPacketsAcked(0) {}
};



class CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB Outduct {
private:
    Outduct();
public:

    OUTDUCT_MANAGER_LIB_EXPORT Outduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    OUTDUCT_MANAGER_LIB_EXPORT virtual ~Outduct();
    virtual std::size_t GetTotalDataSegmentsUnacked() = 0;
    virtual bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t> && userData) = 0;
    virtual bool Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) = 0;
    virtual bool Forward(std::vector<uint8_t> & movableDataVec, std::vector<uint8_t>&& userData) = 0;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetUserAssignedUuid(uint64_t userAssignedUuid);
    virtual void Connect() = 0;
    virtual bool ReadyToForward() = 0;
    virtual void Stop() = 0;
    virtual void GetOutductFinalStats(OutductFinalStats & finalStats) = 0;
    OUTDUCT_MANAGER_LIB_EXPORT virtual uint64_t GetOutductTelemetry(uint8_t* data, uint64_t bufferSize);

    OUTDUCT_MANAGER_LIB_EXPORT uint64_t GetOutductUuid() const;
    OUTDUCT_MANAGER_LIB_EXPORT uint64_t GetOutductMaxNumberOfBundlesInPipeline() const;
    OUTDUCT_MANAGER_LIB_EXPORT uint64_t GetOutductMaxSumOfBundleBytesInPipeline() const;
    OUTDUCT_MANAGER_LIB_EXPORT uint64_t GetOutductNextHopNodeId() const;
    OUTDUCT_MANAGER_LIB_EXPORT std::string GetConvergenceLayerName() const;

protected:
    const outduct_element_config_t m_outductConfig;
    const uint64_t m_outductUuid;
};

#endif // OUTDUCT_H

