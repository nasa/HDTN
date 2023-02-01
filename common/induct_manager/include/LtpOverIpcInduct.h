/**
 * @file LtpOverIpcInduct.h
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
 * The LtpOverIpcInduct class contains the functionality for an LTP induct
 * used by the InductManager.  This class is the interface to ltp_lib.
 */

#ifndef LTP_OVER_IPC_INDUCT_H
#define LTP_OVER_IPC_INDUCT_H 1

#include <string>
#include "LtpInduct.h"
#include "LtpOverIpcBundleSink.h"

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB LtpOverIpcInduct : public LtpInduct {
public:
    INDUCT_MANAGER_LIB_EXPORT LtpOverIpcInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes);
    INDUCT_MANAGER_LIB_EXPORT virtual ~LtpOverIpcInduct() override;
protected:
    INDUCT_MANAGER_LIB_EXPORT virtual bool SetLtpBundleSinkPtr() override;
private:
    LtpOverIpcInduct() = delete;
private:
    std::unique_ptr<LtpOverIpcBundleSink> m_ltpOverIpcBundleSinkPtr;
};


#endif // LTP_OVER_IPC_INDUCT_H

