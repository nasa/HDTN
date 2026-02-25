/**
 * @file LtpOverEncapLocalStreamBundleSink.h
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
 * This LtpOverEncapLocalStreamBundleSink class encapsulates the appropriate LTP functionality
 * to receive bundles (or any other user defined data) over an LTP over Encap Local Stream link
 * and calls the user defined function LtpWholeBundleReadyCallback_t when a new bundle
 * is received.
 */

#ifndef _LTP_OVER_ENCAP_LOCAL_STREAM_BUNDLE_SINK_H
#define _LTP_OVER_ENCAP_LOCAL_STREAM_BUNDLE_SINK_H 1

#include "LtpBundleSink.h"
#include "LtpEncapLocalStreamEngine.h"

class LtpOverEncapLocalStreamBundleSink : public LtpBundleSink {
private:
    LtpOverEncapLocalStreamBundleSink() = delete;
public:

    LTP_LIB_EXPORT LtpOverEncapLocalStreamBundleSink(const LtpWholeBundleReadyCallback_t & ltpWholeBundleReadyCallback, const LtpEngineConfig & ltpRxCfg);
    LTP_LIB_EXPORT virtual ~LtpOverEncapLocalStreamBundleSink() override;
    LTP_LIB_EXPORT virtual bool ReadyToBeDeleted() override;
protected:
    LTP_LIB_EXPORT virtual void GetTransportLayerSpecificTelem(LtpInductConnectionTelemetry_t& telem) const override;
    LTP_LIB_EXPORT virtual bool SetLtpEnginePtr() override;
private:

    //ltp vars
    std::unique_ptr<LtpEncapLocalStreamEngine> m_ltpEncapLocalStreamEnginePtr;
};



#endif  //_LTP_OVER_ENCAP_LOCAL_STREAM_BUNDLE_SINK_H
