/**
 * @file LtpOverIpcOutduct.h
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
 * The LtpOverIpcOutduct class contains the functionality for an LTP over IPC (Interprocess Communication) outduct
 * used by the OutductManager.  This class is the interface to ltp_lib.
 */

#ifndef LTP_OVER_IPC_OUTDUCT_H
#define LTP_OVER_IPC_OUTDUCT_H 1

#include "LtpOutduct.h"
#include "LtpOverIpcBundleSource.h"

class CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB LtpOverIpcOutduct : public LtpOutduct {
public:
    OUTDUCT_MANAGER_LIB_EXPORT LtpOverIpcOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    OUTDUCT_MANAGER_LIB_EXPORT virtual ~LtpOverIpcOutduct() override;
protected:
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool SetLtpBundleSourcePtr() override;
private:
    LtpOverIpcOutduct() = delete;


    std::unique_ptr<LtpOverIpcBundleSource> m_ltpOverIpcBundleSourcePtr;
};


#endif // LTP_OVER_IPC_OUTDUCT_H

