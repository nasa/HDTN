/**
 * @file LtpBundleSink.h
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
    LTP_LIB_EXPORT virtual bool ReadyToBeDeleted() = 0;
protected:
    LTP_LIB_EXPORT virtual bool SetLtpEnginePtr() = 0;
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
public:
    InductConnectionTelemetry_t m_telemetry;
};



#endif  //_LTP_BUNDLE_SINK_H
