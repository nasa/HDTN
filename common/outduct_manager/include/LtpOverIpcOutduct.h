/**
 * @file LtpOverIpcOutduct.h
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

