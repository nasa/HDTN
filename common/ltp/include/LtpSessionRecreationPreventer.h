/**
 * @file LtpSessionRecreationPreventer.h
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
 * This LtpSessionRecreationPreventer class is used to remember a desired number of
 * the most recent previously received LTP session numbers.
 * It was created during testing of sending large UDP packets with IP fragmentation to
 * help mitigate an anomaly that was seen where old closed session numbers would
 * reappear much later during a multi-session transmission.
 */

#ifndef LTP_SESSION_RECREATION_PREVENTER_H
#define LTP_SESSION_RECREATION_PREVENTER_H 1

#include <cstdint>
#include <vector>
#include <unordered_set>
#include "ltp_lib_export.h"
#include <boost/core/noncopyable.hpp>

class LtpSessionRecreationPreventer : private boost::noncopyable {
private:
    LtpSessionRecreationPreventer() = delete;
public:
    
    /**
     * Preallocate M_NUM_RECEIVED_SESSION_NUMBERS_TO_REMEMBER number of elements for quarantine set and queue.
     * Start circular write index from 0.
     * @param numReceivedSessionsToRemember The number of session numbers to remember.
     */
    LTP_LIB_EXPORT LtpSessionRecreationPreventer(const uint64_t numReceivedSessionsToRemember);
    
    /// Default destructor.
    LTP_LIB_EXPORT ~LtpSessionRecreationPreventer();
    
    /** Add session number to quarantine.
     *
     * @param newSessionNumber The session number to add.
     * @return True if the session number was added successfully, or False otherwise.
     * @post If returning True and max capacity would have been exceeded, the session number is added to quarantine and the oldest session number is evicted.
     */
    LTP_LIB_EXPORT bool AddSession(const uint64_t newSessionNumber);
    
    /** Query whether quarantine contains session number.
     *
     * @param newSessionNumber The session number to query.
     * @return True if quarantine contains session number, or False otherwise.
     */
    LTP_LIB_EXPORT bool ContainsSession(const uint64_t newSessionNumber) const;

private:
    /// Maximum number of session numbers to remember
    const uint64_t M_NUM_RECEIVED_SESSION_NUMBERS_TO_REMEMBER;
    /// Session number quarantine set, for fast lookup
    std::unordered_set<uint64_t> m_previouslyReceivedSessionNumbersUnorderedSet;
    /// Session number quarantine queue, evicts oldest -> newest on push when full
    std::vector<uint64_t> m_previouslyReceivedSessionNumbersQueueVector;
    /// Whether m_previouslyReceivedSessionNumbersQueueVector is currently full
    bool m_queueIsFull;
    /// Circular write index, used by m_previouslyReceivedSessionNumbersQueueVector
    uint64_t m_nextQueueIndex;
};

#endif // LTP_SESSION_RECREATION_PREVENTER_H

