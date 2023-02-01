/**
 * @file LtpOverIpcOutduct.cpp
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
 */

#include "LtpOverIpcOutduct.h"
#include <boost/make_unique.hpp>

LtpOverIpcOutduct::LtpOverIpcOutduct(const outduct_element_config_t& outductConfig, const uint64_t outductUuid) :
    LtpOutduct(outductConfig, outductUuid)
{
}
LtpOverIpcOutduct::~LtpOverIpcOutduct() {}

bool LtpOverIpcOutduct::SetLtpBundleSourcePtr() {
    m_ltpOverIpcBundleSourcePtr = boost::make_unique<LtpOverIpcBundleSource>(m_ltpTxCfg);
    if (!m_ltpOverIpcBundleSourcePtr->Init()) {
        return false;
    }
    m_ltpBundleSourcePtr = m_ltpOverIpcBundleSourcePtr.get();
    return true;
}
