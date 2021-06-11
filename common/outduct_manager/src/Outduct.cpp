#include "Outduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>


Outduct::Outduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid) :
    m_outductConfig(outductConfig),
    m_outductUuid(outductUuid)
{}
Outduct::~Outduct() {}

uint64_t Outduct::GetOutductUuid() const {
    return m_outductUuid;
}
uint64_t Outduct::GetOutductMaxBundlesInPipeline() const {
    return m_outductConfig.bundlePipelineLimit;
}