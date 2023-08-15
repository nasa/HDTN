/**
 * @file LtpOverUdpBundleSource.h
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
