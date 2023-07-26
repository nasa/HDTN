#include "codec/Bpv6FragmentManager.h"
#include "Logger.h"
#include "codec/Bpv6Fragment.h"
#include "codec/bpv6.h"
#include <boost/thread/mutex.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static bool AddBundle(BundleViewV6 & bv, uint8_t *data, size_t len, uint64_t &payloadLen) {
    if(!bv.LoadBundle(data, len)) {
        LOG_ERROR(subprocess) << "Failed to load full bundle";
        return false;
    }

    if(!bv.GetPayloadSize(bv, payloadLen)) {
        LOG_ERROR(subprocess) << "Failed to get payload length";
        return false;
    }

    if(!bv.Render(len + 1024)) {
        LOG_ERROR(subprocess) << "Failed to render bundle";
        return false;
    }
    return true;
}

bool Bpv6FragmentManager::AddFragmentAndGetComplete(uint8_t *data, size_t len, bool & isComplete, BundleViewV6 & assembledBv) {
    isComplete = false;

    BundleViewV6 bvHdr;
    if(!bvHdr.LoadBundle(data, len, true)) {
        LOG_ERROR(subprocess) << "Failed to load bundle header";
        return false;
    }
    Bpv6CbhePrimaryBlock & primary = bvHdr.m_primaryBlockView.header;

    if(!primary.HasFragmentationFlagSet()) {
        LOG_ERROR(subprocess) << "Not a fragment";
        return false;
    }

    Bpv6Id id{primary.m_sourceNodeId, primary.m_creationTimestamp};

    FragmentInfo & info = m_idToFrags[id];

    BundleViewV6 & bv = info.bundles.emplace_back();
    uint64_t payloadLen = 0;

    if(!AddBundle(bv, data, len, payloadLen)) {
        LOG_ERROR(subprocess) << "Failed to add bundle to fragment manager";
        info.bundles.pop_back();
        return false;
    }

    FragmentSet::data_fragment_t rng(
            primary.m_fragmentOffset, 
            primary.m_fragmentOffset + payloadLen);

    FragmentSet::data_fragment_t full(
            0, 
            primary.m_totalApplicationDataUnitLength);

    FragmentSet::InsertFragment(info.fragmentSet, rng);

    if(!FragmentSet::ContainsFragmentEntirely(info.fragmentSet,full)) {
        // We've saved the fragment
        return true;
    }

    bool success = AssembleFragments(info.bundles, assembledBv);

    m_idToFrags.erase(id);
    isComplete = success;
    return success;
}

bool Bpv6FragmentManager::AddFragmentAndGetComplete_ThreadSafe(uint8_t *data, size_t len, bool & isComplete, BundleViewV6 & assembledBv) {
    boost::mutex::scoped_lock lock(m_mutex);
    return AddFragmentAndGetComplete(data, len, isComplete, assembledBv);
}

bool Bpv6FragmentManager::Bpv6Id::operator<(const Bpv6Id &o) const {
    if(src.nodeId == o.src.nodeId) {
        if(src.serviceId == o.src.serviceId) {
            return ts < o.ts;
        }
        return src.serviceId < o.src.serviceId;
    }
    return src.nodeId < o.src.nodeId;
}

