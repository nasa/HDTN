/**
 * @file LtpOverUdpOutduct.h
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
 * The LtpOverUdpOutduct class contains the functionality for an LTP over UDP outduct
 * used by the OutductManager.  This class is the interface to ltp_lib.
 */

#ifndef LTP_OVER_UDP_OUTDUCT_H
#define LTP_OVER_UDP_OUTDUCT_H 1

#include "LtpOutduct.h"
#include "LtpOverUdpBundleSource.h"

class CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB LtpOverUdpOutduct : public LtpOutduct {
public:
    OUTDUCT_MANAGER_LIB_EXPORT LtpOverUdpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid);
    OUTDUCT_MANAGER_LIB_EXPORT virtual ~LtpOverUdpOutduct() override;
protected:
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool SetLtpBundleSourcePtr() override;
private:
    LtpOverUdpOutduct() = delete;


    std::unique_ptr<LtpOverUdpBundleSource> m_ltpOverUdpBundleSourcePtr;
};


#endif // LTP_OVER_UDP_OUTDUCT_H

