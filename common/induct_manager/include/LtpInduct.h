/**
 * @file LtpInduct.h
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
 * The LtpInduct class contains the functionality for an LTP induct
 * used by the InductManager.  This class is the interface to ltp_lib.
 */

#ifndef LTP_INDUCT_H
#define LTP_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "LtpEngineConfig.h"
#include "LtpBundleSink.h"

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB LtpInduct : public Induct {
public:
    INDUCT_MANAGER_LIB_EXPORT LtpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes);
    INDUCT_MANAGER_LIB_EXPORT virtual ~LtpInduct() override;
    INDUCT_MANAGER_LIB_EXPORT virtual bool Init() override; //override NOOP base class
    INDUCT_MANAGER_LIB_EXPORT virtual void PopulateInductTelemetry(InductTelemetry_t& inductTelem) override;
private:
    LtpInduct() = delete;
protected:
    INDUCT_MANAGER_LIB_EXPORT virtual bool SetLtpBundleSinkPtr() = 0;
protected:
    LtpBundleSink* m_ltpBundleSinkPtr;
    LtpEngineConfig m_ltpRxCfg;
};


#endif // LTP_INDUCT_H

