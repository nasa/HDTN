/**
 * @file LtpOverEncapLocalStreamBundleSink.h
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
