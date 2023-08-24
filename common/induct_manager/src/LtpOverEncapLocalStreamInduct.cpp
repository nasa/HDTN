/**
 * @file LtpOverEncapLocalStreamInduct.cpp
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

#include "LtpOverEncapLocalStreamInduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include "LtpEngineConfig.h"

LtpOverEncapLocalStreamInduct::LtpOverEncapLocalStreamInduct(
    const InductProcessBundleCallback_t & inductProcessBundleCallback,
    const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes) :
    LtpInduct(inductProcessBundleCallback, inductConfig, maxBundleSizeBytes)
{
}
LtpOverEncapLocalStreamInduct::~LtpOverEncapLocalStreamInduct() {}

bool LtpOverEncapLocalStreamInduct::SetLtpBundleSinkPtr() {
    m_ltpOverEncapLocalStreamBundleSinkPtr = boost::make_unique<LtpOverEncapLocalStreamBundleSink>(m_inductProcessBundleCallback, m_ltpRxCfg);
    if (!m_ltpOverEncapLocalStreamBundleSinkPtr->Init()) {
        return false;
    }
    m_ltpBundleSinkPtr = m_ltpOverEncapLocalStreamBundleSinkPtr.get();
    return true;
}
