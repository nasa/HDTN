/**
 * @file LtpOverIpcBundleSource.h
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
 * This LtpOverIpcBundleSource class encapsulates the appropriate LTP functionality
 * to send a pipeline of bundles (or any other user defined data) over an LTP over IPC (Interprocess Communication) link
 * and calls the user defined function OnSuccessfulAckCallback_t when the session closes, meaning
 * a bundle is fully sent (i.e. the ltp fully red session gets acknowledged by the remote receiver).
 */

#ifndef _LTP_OVER_IPC_BUNDLE_SOURCE_H
#define _LTP_OVER_IPC_BUNDLE_SOURCE_H 1

#include "LtpBundleSource.h"
#include "LtpIpcEngine.h"

class LtpOverIpcBundleSource : public LtpBundleSource {
private:
    LtpOverIpcBundleSource() = delete;
public:
    LTP_LIB_EXPORT LtpOverIpcBundleSource(const LtpEngineConfig& ltpTxCfg);
    LTP_LIB_EXPORT virtual ~LtpOverIpcBundleSource() override;
protected:
    LTP_LIB_EXPORT virtual bool ReadyToForward() override;
    LTP_LIB_EXPORT virtual bool SetLtpEnginePtr() override;
    LTP_LIB_EXPORT virtual void GetTransportLayerSpecificTelem(LtpOutductTelemetry_t& telem) const override;

private:
    //ltp vars
    std::unique_ptr<LtpIpcEngine> m_ltpIpcEnginePtr;
};



#endif //_LTP_OVER_IPC_BUNDLE_SOURCE_H
