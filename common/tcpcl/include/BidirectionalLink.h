/**
 * @file BidirectionalLink.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @section DESCRIPTION
 *
 * This BidirectionalLink pure virtual base class defines common functionality
 * that is shared between versions 3 and 4 of the TCP Convergence-Layer Protocol.
 */

#ifndef _BIDIRECTIONAL_LINK_H
#define _BIDIRECTIONAL_LINK_H 1
#include "tcpcl_lib_export.h"
#ifndef CLASS_VISIBILITY_TCPCL_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_TCPCL_LIB
#  else
#    define CLASS_VISIBILITY_TCPCL_LIB TCPCL_LIB_EXPORT
#  endif
#endif
#include <cstdint>
#include <atomic>
#include <boost/config/detail/suffix.hpp>

struct BidirectionalLinkAtomicTelem_t {
    BidirectionalLinkAtomicTelem_t() :
        totalFragmentsReceived(0),
        totalBundlesReceived(0),
        totalBundleBytesReceived(0),
        totalFragmentsSent(0),
        totalFragmentsSentAndAcked(0),
        totalBundlesSent(0),
        totalBundlesSentAndAcked(0),
        totalBundleBytesSent(0),
        totalBundleBytesSentAndAcked(0),
        totalBundlesFailedToSend(0),
        numTcpReconnectAttempts(0),
        linkIsUpPhysically(false) {}

    //telemetry
    std::atomic<uint64_t> totalFragmentsReceived;
    //uint64_t m_totalFragmentsReceivedAndAcked; //don't care about count of acks sent on rx fragments

    std::atomic<uint64_t> totalBundlesReceived;
    //uint64_t m_totalBundlesReceivedAndAcked; //don't care about count of acks sent on rx bundles

    std::atomic<uint64_t> totalBundleBytesReceived;

    std::atomic<uint64_t> totalFragmentsSent;
    std::atomic<uint64_t> totalFragmentsSentAndAcked;

    std::atomic<uint64_t> totalBundlesSent;
    std::atomic<uint64_t> totalBundlesSentAndAcked;

    std::atomic<uint64_t> totalBundleBytesSent;
    std::atomic<uint64_t> totalBundleBytesSentAndAcked;

    std::atomic<uint64_t> totalBundlesFailedToSend;

    std::atomic<uint64_t> numTcpReconnectAttempts;

    std::atomic<bool> linkIsUpPhysically;
};

class BidirectionalLink {
public:
    TCPCL_LIB_EXPORT BidirectionalLink() : m_base_telem() {}

    BOOST_FORCEINLINE std::size_t BaseClass_GetTotalBundlesAcked() const noexcept {
        return m_base_telem.totalBundlesSentAndAcked.load(std::memory_order_acquire);
    }

    BOOST_FORCEINLINE std::size_t BaseClass_GetTotalBundlesSent() const noexcept {
        return m_base_telem.totalBundlesSent.load(std::memory_order_acquire);
    }

    BOOST_FORCEINLINE std::size_t BaseClass_GetTotalBundlesUnacked() const noexcept {
        return m_base_telem.totalBundlesSent.load(std::memory_order_acquire) - m_base_telem.totalBundlesSentAndAcked.load(std::memory_order_acquire);
    }

    BOOST_FORCEINLINE std::size_t BaseClass_GetTotalBundleBytesAcked() const noexcept {
        return m_base_telem.totalBundleBytesSentAndAcked.load(std::memory_order_acquire);
    }

    BOOST_FORCEINLINE std::size_t BaseClass_GetTotalBundleBytesSent() const noexcept {
        return m_base_telem.totalBundleBytesSent.load(std::memory_order_acquire);
    }

    BOOST_FORCEINLINE std::size_t BaseClass_GetTotalBundleBytesUnacked() const noexcept {
        return m_base_telem.totalBundleBytesSent.load(std::memory_order_acquire) - m_base_telem.totalBundleBytesSentAndAcked.load(std::memory_order_acquire);
    }

    virtual unsigned int Virtual_GetMaxTxBundlesInPipeline() = 0;

    BidirectionalLinkAtomicTelem_t m_base_telem;
};



#endif  //_BIDIRECTIONAL_LINK_H
