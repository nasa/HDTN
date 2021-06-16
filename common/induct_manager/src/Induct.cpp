#include "Induct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>

//INDUCT
Induct::Induct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig) :
    m_inductProcessBundleCallback(inductProcessBundleCallback),
    m_inductConfig(inductConfig)
{}
Induct::~Induct() {}
