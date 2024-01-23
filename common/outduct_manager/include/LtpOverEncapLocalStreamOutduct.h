/**
 * @file LtpOverEncapLocalStreamOutduct.h
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
 * The LtpOverEncapLocalStreamOutduct class contains the functionality for an LTP over EncapLocalStream outduct
 * used by the OutductManager.  This class is the interface to ltp_lib.
 */

#ifndef LTP_OVER_ENCAP_LOCAL_STREAM_OUTDUCT_H
#define LTP_OVER_ENCAP_LOCAL_STREAM_OUTDUCT_H 1

#include "LtpOutduct.h"
#include "LtpOverEncapLocalStreamBundleSource.h"

class CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB LtpOverEncapLocalStreamOutduct : public LtpOutduct {
public:
    OUTDUCT_MANAGER_LIB_EXPORT LtpOverEncapLocalStreamOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    OUTDUCT_MANAGER_LIB_EXPORT virtual ~LtpOverEncapLocalStreamOutduct() override;
protected:
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool SetLtpBundleSourcePtr() override;
private:
    LtpOverEncapLocalStreamOutduct() = delete;


    std::unique_ptr<LtpOverEncapLocalStreamBundleSource> m_ltpOverEncapLocalStreamBundleSourcePtr;
};


#endif // LTP_OVER_ENCAP_LOCAL_STREAM_OUTDUCT_H

