/**
 * @file LtpOverUdpBundleSink.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
 * This LtpOverUdpBundleSink class encapsulates the appropriate LTP functionality
 * to receive bundles (or any other user defined data) over an LTP over UDP link
 * and calls the user defined function LtpWholeBundleReadyCallback_t when a new bundle
 * is received.
 */

#ifndef _LTP_OVER_UDP_BUNDLE_SINK_H
#define _LTP_OVER_UDP_BUNDLE_SINK_H 1

#include "LtpBundleSink.h"
#include "LtpUdpEngineManager.h"
#include <atomic>

class LtpOverUdpBundleSink : public LtpBundleSink {
private:
    LtpOverUdpBundleSink() = delete;
public:

    LTP_LIB_EXPORT LtpOverUdpBundleSink(const LtpWholeBundleReadyCallback_t & ltpWholeBundleReadyCallback, const LtpEngineConfig & ltpRxCfg);
    LTP_LIB_EXPORT virtual ~LtpOverUdpBundleSink() override;
    LTP_LIB_EXPORT virtual bool ReadyToBeDeleted() override;
protected:
    LTP_LIB_EXPORT virtual void GetTransportLayerSpecificTelem(LtpInductConnectionTelemetry_t& telem) const override;
    LTP_LIB_EXPORT virtual bool SetLtpEnginePtr() override;
private:
    LTP_LIB_NO_EXPORT void RemoveCallback();

    //ltp vars
    std::shared_ptr<LtpUdpEngineManager> m_ltpUdpEngineManagerPtr;
    LtpUdpEngine * m_ltpUdpEnginePtr;

    boost::mutex m_removeEngineMutex;
    boost::condition_variable m_removeEngineCv;
    std::atomic<bool> m_removeEngineInProgress;
};



#endif  //_LTP_OVER_UDP_BUNDLE_SINK_H
