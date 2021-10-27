#ifndef INDUCT_H
#define INDUCT_H 1

#include <string>
#include <boost/integer.hpp>
#include <boost/function.hpp>
#include "InductsConfig.h"
#include <list>

class Induct;
typedef boost::function<void(std::vector<uint8_t> & movableBundle)> InductProcessBundleCallback_t;
typedef boost::function<void(const uint64_t remoteNodeId, Induct* thisInductPtr)> OnNewOpportunisticLinkCallback_t;
typedef boost::function<void(const uint64_t remoteNodeId)> OnDeletedOpportunisticLinkCallback_t;

class Induct {
private:
    Induct();
public:
    Induct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig);
    virtual ~Induct();

protected:
    const InductProcessBundleCallback_t m_inductProcessBundleCallback;
    const induct_element_config_t m_inductConfig;
};

#endif // INDUCT_H

