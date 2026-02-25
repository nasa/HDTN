/**
 * @file LtpOverEncapLocalStreamInduct.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
