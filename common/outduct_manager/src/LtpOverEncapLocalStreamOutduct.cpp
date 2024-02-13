/**
 * @file LtpOverEncapLocalStreamOutduct.cpp
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
 */

#include "LtpOverEncapLocalStreamOutduct.h"
#include <boost/make_unique.hpp>

LtpOverEncapLocalStreamOutduct::LtpOverEncapLocalStreamOutduct(const outduct_element_config_t& outductConfig, const uint64_t outductUuid) :
    LtpOutduct(outductConfig, outductUuid)
{
}
LtpOverEncapLocalStreamOutduct::~LtpOverEncapLocalStreamOutduct() {}

bool LtpOverEncapLocalStreamOutduct::SetLtpBundleSourcePtr() {
    m_ltpOverEncapLocalStreamBundleSourcePtr = boost::make_unique<LtpOverEncapLocalStreamBundleSource>(m_ltpTxCfg);
    if (!m_ltpOverEncapLocalStreamBundleSourcePtr->Init()) {
        return false;
    }
    m_ltpBundleSourcePtr = m_ltpOverEncapLocalStreamBundleSourcePtr.get();
    return true;
}
