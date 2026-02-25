/**
 * @file LtpOverUdpInduct.h
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
 *
 * @section DESCRIPTION
 *
 * The LtpOverUdpInduct class contains the functionality for an LTP induct
 * used by the InductManager.  This class is the interface to ltp_lib.
 */

#ifndef LTP_OVER_UDP_INDUCT_H
#define LTP_OVER_UDP_INDUCT_H 1

#include <string>
#include "LtpInduct.h"
#include "LtpOverUdpBundleSink.h"

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB LtpOverUdpInduct : public LtpInduct {
public:
    INDUCT_MANAGER_LIB_EXPORT LtpOverUdpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes);
    INDUCT_MANAGER_LIB_EXPORT virtual ~LtpOverUdpInduct() override;
protected:
    INDUCT_MANAGER_LIB_EXPORT virtual bool SetLtpBundleSinkPtr() override;
private:
    LtpOverUdpInduct() = delete;
private:
    std::unique_ptr<LtpOverUdpBundleSink> m_ltpOverUdpBundleSinkPtr;
};


#endif // LTP_OVER_UDP_INDUCT_H

