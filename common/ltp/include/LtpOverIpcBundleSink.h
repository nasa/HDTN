/**
 * @file LtpOverIpcBundleSink.h
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
 * This LtpOverIpcBundleSink class encapsulates the appropriate LTP functionality
 * to receive bundles (or any other user defined data) over an LTP over IPC (Interprocess Communication) link
 * and calls the user defined function LtpWholeBundleReadyCallback_t when a new bundle
 * is received.
 */

#ifndef _LTP_OVER_IPC_BUNDLE_SINK_H
#define _LTP_OVER_IPC_BUNDLE_SINK_H 1

#include "LtpBundleSink.h"
#include "LtpIpcEngine.h"

class LtpOverIpcBundleSink : public LtpBundleSink {
private:
    LtpOverIpcBundleSink() = delete;
public:

    LTP_LIB_EXPORT LtpOverIpcBundleSink(const LtpWholeBundleReadyCallback_t & ltpWholeBundleReadyCallback, const LtpEngineConfig & ltpRxCfg);
    LTP_LIB_EXPORT virtual ~LtpOverIpcBundleSink() override;
    LTP_LIB_EXPORT virtual bool ReadyToBeDeleted() override;
protected:
    LTP_LIB_EXPORT virtual void GetTransportLayerSpecificTelem(LtpInductConnectionTelemetry_t& telem) const override;
    LTP_LIB_EXPORT virtual bool SetLtpEnginePtr() override;
private:

    //ltp vars
    std::unique_ptr<LtpIpcEngine> m_ltpIpcEnginePtr;
};



#endif  //_LTP_OVER_IPC_BUNDLE_SINK_H
