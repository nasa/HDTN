/**
 * @file LtpOverEncapLocalStreamInduct.h
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
 * The LtpOverEncapLocalStreamInduct class contains the functionality for an LTP induct
 * used by the InductManager.  This class is the interface to ltp_lib.
 */

#ifndef LTP_OVER_ENCAP_LOCAL_STREAM_INDUCT_H
#define LTP_OVER_ENCAP_LOCAL_STREAM_INDUCT_H 1

#include <string>
#include "LtpInduct.h"
#include "LtpOverEncapLocalStreamBundleSink.h"

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB LtpOverEncapLocalStreamInduct : public LtpInduct {
public:
    INDUCT_MANAGER_LIB_EXPORT LtpOverEncapLocalStreamInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes);
    INDUCT_MANAGER_LIB_EXPORT virtual ~LtpOverEncapLocalStreamInduct() override;
protected:
    INDUCT_MANAGER_LIB_EXPORT virtual bool SetLtpBundleSinkPtr() override;
private:
    LtpOverEncapLocalStreamInduct() = delete;
private:
    std::unique_ptr<LtpOverEncapLocalStreamBundleSink> m_ltpOverEncapLocalStreamBundleSinkPtr;
};


#endif // LTP_OVER_ENCAP_LOCAL_STREAM_INDUCT_H

