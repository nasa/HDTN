/**
 * @file SlipOverUartInduct.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @section DESCRIPTION
 *
 * The SlipOverUartInduct class contains the functionality for a SLIP over UART induct
 * used by the InductManager.  This class is the interface to slip_over_uart_lib.
 */

#ifndef SLIP_OVER_UART_INDUCT_H
#define SLIP_OVER_UART_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "UartInterface.h"
#include <list>
#include <boost/make_unique.hpp>
#include <memory>

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB SlipOverUartInduct : public Induct {
public:
    INDUCT_MANAGER_LIB_EXPORT SlipOverUartInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
        const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
        const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback);
    INDUCT_MANAGER_LIB_EXPORT virtual ~SlipOverUartInduct() override;
    INDUCT_MANAGER_LIB_EXPORT virtual void PopulateInductTelemetry(InductTelemetry_t& inductTelem) override;
private:
    

    SlipOverUartInduct() = delete;
    
    INDUCT_MANAGER_LIB_NO_EXPORT void NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);
    INDUCT_MANAGER_LIB_EXPORT virtual void Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) override;

    INDUCT_MANAGER_LIB_NO_EXPORT void OnFailedBundleVecSendCallback(padded_vector_uint8_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled);
    INDUCT_MANAGER_LIB_NO_EXPORT void OnFailedBundleZmqSendCallback(zmq::message_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled);
    INDUCT_MANAGER_LIB_NO_EXPORT void OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid);

    UartInterface m_uartInterface;
    OpportunisticBundleQueue* m_opportunisticBundleQueuePtr;
};


#endif // SLIP_OVER_UART_INDUCT_H

