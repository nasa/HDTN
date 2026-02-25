/**
 * @file LtpBundleSink.h
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
 * This LtpBundleSink class encapsulates the appropriate LTP functionality
 * to receive bundles (or any other user defined data) over an LTP link (transport layer must be defined in child class)
 * and calls the user defined function LtpWholeBundleReadyCallback_t when a new bundle
 * is received.
 */

#ifndef _LTP_BUNDLE_SINK_H
#define _LTP_BUNDLE_SINK_H 1

#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include "LtpEngine.h"
#include "LtpEngineConfig.h"
#include "TelemetryDefinitions.h"
#include "PaddedVectorUint8.h"
#include <boost/core/noncopyable.hpp>

class LtpBundleSink : private boost::noncopyable {
private:
    LtpBundleSink() = delete;
public:
    typedef boost::function<void(padded_vector_uint8_t & wholeBundleVec)> LtpWholeBundleReadyCallback_t;

    LTP_LIB_EXPORT LtpBundleSink(const LtpWholeBundleReadyCallback_t & ltpWholeBundleReadyCallback, const LtpEngineConfig & ltpRxCfg);
    LTP_LIB_EXPORT virtual ~LtpBundleSink();
    LTP_LIB_EXPORT bool Init();
    LTP_LIB_EXPORT void GetTelemetry(LtpInductConnectionTelemetry_t& telem) const;
    LTP_LIB_EXPORT virtual bool ReadyToBeDeleted() = 0;
protected:
    LTP_LIB_EXPORT virtual bool SetLtpEnginePtr() = 0;
    LTP_LIB_EXPORT virtual void GetTransportLayerSpecificTelem(LtpInductConnectionTelemetry_t& telem) const = 0;
private:

    //tcpcl received data callback functions
    LTP_LIB_NO_EXPORT void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, padded_vector_uint8_t & movableClientServiceDataVec,
        uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock);
    LTP_LIB_NO_EXPORT void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode);

    const LtpWholeBundleReadyCallback_t m_ltpWholeBundleReadyCallback;
protected:
    //ltp vars
    const LtpEngineConfig m_ltpRxCfg;
    const uint64_t M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;
    LtpEngine * m_ltpEnginePtr;

    //telemetry
    const std::string M_CONNECTION_NAME;
    const std::string M_INPUT_NAME;
    std::atomic<uint64_t> m_totalBundlesReceived;
    std::atomic<uint64_t> m_totalBundleBytesReceived;
};



#endif  //_LTP_BUNDLE_SINK_H
