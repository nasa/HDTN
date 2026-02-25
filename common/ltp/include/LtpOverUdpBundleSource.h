/**
 * @file LtpOverUdpBundleSource.h
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
 * This LtpOverUdpBundleSource class encapsulates the appropriate LTP functionality
 * to send a pipeline of bundles (or any other user defined data) over an LTP over UDP link
 * and calls the user defined function OnSuccessfulAckCallback_t when the session closes, meaning
 * a bundle is fully sent (i.e. the ltp fully red session gets acknowledged by the remote receiver).
 */

#ifndef _LTP_OVER_UDP_BUNDLE_SOURCE_H
#define _LTP_OVER_UDP_BUNDLE_SOURCE_H 1

#include "LtpBundleSource.h"
#include "LtpUdpEngineManager.h"
#include <atomic>

class LtpOverUdpBundleSource : public LtpBundleSource {
private:
    LtpOverUdpBundleSource() = delete;
public:
    LTP_LIB_EXPORT LtpOverUdpBundleSource(const LtpEngineConfig& ltpTxCfg);
    LTP_LIB_EXPORT virtual ~LtpOverUdpBundleSource() override;
protected:
    LTP_LIB_EXPORT virtual bool ReadyToForward() override;
    LTP_LIB_EXPORT virtual bool SetLtpEnginePtr() override;
    LTP_LIB_EXPORT virtual void GetTransportLayerSpecificTelem(LtpOutductTelemetry_t& telem) const override;
private:
    LTP_LIB_NO_EXPORT void RemoveCallback();

private:
    std::shared_ptr<LtpUdpEngineManager> m_ltpUdpEngineManagerPtr;
    LtpUdpEngine* m_ltpUdpEnginePtr;

    boost::mutex m_removeEngineMutex;
    boost::condition_variable m_removeEngineCv;
    std::atomic<bool> m_removeEngineInProgress;
};



#endif //_LTP_OVER_UDP_BUNDLE_SOURCE_H
