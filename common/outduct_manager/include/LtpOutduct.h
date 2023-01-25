/**
 * @file LtpOutduct.h
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
 * The LtpOutduct class contains the functionality for an LTP outduct
 * used by the OutductManager.  This class is the interface to ltp_lib.
 * The derived class must implement the transport layer that LTP will run on.
 */

#ifndef LTP_OUTDUCT_H
#define LTP_OUTDUCT_H 1

#include <string>
#include <memory>
#include "Outduct.h"
#include "LtpBundleSource.h"
#include <list>
#include "LtpEngineConfig.h"

class CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB LtpOutduct : public Outduct {
public:
    OUTDUCT_MANAGER_LIB_EXPORT LtpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    OUTDUCT_MANAGER_LIB_EXPORT virtual ~LtpOutduct() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Init() override; //override NOOP base class
    OUTDUCT_MANAGER_LIB_EXPORT virtual std::size_t GetTotalDataSegmentsUnacked() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(std::vector<uint8_t> & movableDataVec, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetUserAssignedUuid(uint64_t userAssignedUuid) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual uint64_t GetOutductMaxNumberOfBundlesInPipeline() const override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void Connect() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool ReadyToForward() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void Stop() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void GetOutductFinalStats(OutductFinalStats & finalStats) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual uint64_t GetOutductTelemetry(uint8_t* data, uint64_t bufferSize) override;
protected:
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool SetLtpBundleSourcePtr() = 0;

private:
    LtpOutduct() = delete;

protected:
    LtpBundleSource* m_ltpBundleSourcePtr;
    LtpEngineConfig m_ltpTxCfg;
};


#endif // LTP_OUTDUCT_H

