#ifndef LTP_OVER_UDP_INDUCT_H
#define LTP_OVER_UDP_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "LtpBundleSink.h"

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB LtpOverUdpInduct : public Induct {
public:
    INDUCT_MANAGER_LIB_EXPORT LtpOverUdpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes);
    INDUCT_MANAGER_LIB_EXPORT virtual ~LtpOverUdpInduct();
    
private:
    LtpOverUdpInduct();

    std::unique_ptr<LtpBundleSink> m_ltpBundleSinkPtr;
};


#endif // LTP_OVER_UDP_INDUCT_H

