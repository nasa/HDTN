/**
 * @file UdpOutduct.h
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
 * The UdpOutduct class contains the functionality for a UDP outduct
 * used by the OutductManager.  This class is the interface to udp_lib.
 */

#ifndef UDP_OUTDUCT_H
#define UDP_OUTDUCT_H 1

#include <string>
#include "Outduct.h"
#include "UdpBundleSource.h"
#include <list>

class CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB UdpOutduct : public Outduct {
public:
    OUTDUCT_MANAGER_LIB_EXPORT UdpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    OUTDUCT_MANAGER_LIB_EXPORT virtual ~UdpOutduct() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void PopulateOutductTelemetry(std::unique_ptr<OutductTelemetry_t>& outductTelem) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual std::size_t GetTotalDataSegmentsUnacked() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(padded_vector_uint8_t& movableDataVec, std::vector<uint8_t>&& userData) override;

    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetUserAssignedUuid(uint64_t userAssignedUuid) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetRate(uint64_t maxSendRateBitsPerSecOrZeroToDisable) override;

    OUTDUCT_MANAGER_LIB_EXPORT virtual void Connect() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool ReadyToForward() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void Stop() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void GetOutductFinalStats(OutductFinalStats & finalStats) override;

private:
    UdpOutduct();


    UdpBundleSource m_udpBundleSource;
};


#endif // UDP_OUTDUCT_H

