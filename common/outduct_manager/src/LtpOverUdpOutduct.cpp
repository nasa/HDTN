/**
 * @file LtpOverUdpOutduct.cpp
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

#include "LtpOverUdpOutduct.h"
#include <boost/make_unique.hpp>

LtpOverUdpOutduct::LtpOverUdpOutduct(const outduct_element_config_t& outductConfig, const uint64_t outductUuid) :
    LtpOutduct(outductConfig, outductUuid)
{
}
LtpOverUdpOutduct::~LtpOverUdpOutduct() {}

bool LtpOverUdpOutduct::SetLtpBundleSourcePtr() {
    m_ltpOverUdpBundleSourcePtr = boost::make_unique<LtpOverUdpBundleSource>(m_ltpTxCfg);
    if (!m_ltpOverUdpBundleSourcePtr->Init()) {
        return false;
    }
    m_ltpBundleSourcePtr = m_ltpOverUdpBundleSourcePtr.get();
    return true;
}
