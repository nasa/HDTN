/**
 * @file LtpOverIpcBundleSource.h
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
    LTP_LIB_EXPORT virtual void SyncTransportLayerSpecificTelem() override;

private:
    //ltp vars
    std::unique_ptr<LtpIpcEngine> m_ltpIpcEnginePtr;
};



#endif //_LTP_OVER_IPC_BUNDLE_SOURCE_H
