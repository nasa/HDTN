/**
 * @file Outduct.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 * The Outduct class is the base class for all HDTN outducts
 * which are used by the OutductManager.
 */

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
#include "TelemetryDefinitions.h"
#include "PaddedVectorUint8.h"

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
    virtual void PopulateOutductTelemetry(std::unique_ptr<OutductTelemetry_t>& outductTelem) = 0;
    virtual std::size_t GetTotalDataSegmentsUnacked() = 0;
    virtual bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t> && userData) = 0;
    virtual bool Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) = 0;
    virtual bool Forward(padded_vector_uint8_t& movableDataVec, std::vector<uint8_t>&& userData) = 0;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetUserAssignedUuid(uint64_t userAssignedUuid);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetRate(uint64_t maxSendRateBitsPerSecOrZeroToDisable);
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Init(); //optional
    virtual void Connect() = 0;
    virtual bool ReadyToForward() = 0;
    virtual void Stop() = 0;
    virtual void GetOutductFinalStats(OutductFinalStats & finalStats) = 0;

    OUTDUCT_MANAGER_LIB_EXPORT uint64_t GetOutductUuid() const;
    OUTDUCT_MANAGER_LIB_EXPORT virtual uint64_t GetOutductMaxNumberOfBundlesInPipeline() const;
    OUTDUCT_MANAGER_LIB_EXPORT uint64_t GetOutductMaxSumOfBundleBytesInPipeline() const;
    OUTDUCT_MANAGER_LIB_EXPORT uint64_t GetOutductNextHopNodeId() const;
    OUTDUCT_MANAGER_LIB_EXPORT virtual uint64_t GetStartingMaxSendRateBitsPerSec() const noexcept;
    OUTDUCT_MANAGER_LIB_EXPORT std::string GetConvergenceLayerName() const;
    OUTDUCT_MANAGER_LIB_EXPORT bool GetHasInitLinkState() const;

protected:
    OUTDUCT_MANAGER_LIB_EXPORT Outduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid, const bool hasInitLinkState);
    const outduct_element_config_t m_outductConfig;
    const uint64_t m_outductUuid;
    const bool m_hasInitLinkState;
public:
    bool m_linkIsUpPerTimeSchedule;
    bool m_physicalLinkStatusIsKnown;
    bool m_linkIsUpPhysically;
};

#endif // OUTDUCT_H

