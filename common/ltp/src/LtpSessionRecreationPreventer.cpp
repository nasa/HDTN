/**
 * @file LtpSessionRecreationPreventer.cpp
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
 */

#include "LtpSessionRecreationPreventer.h"
#include "Logger.h"

LtpSessionRecreationPreventer::LtpSessionRecreationPreventer(const uint64_t numReceivedSessionsToRemember) :
    M_NUM_RECEIVED_SESSION_NUMBERS_TO_REMEMBER(numReceivedSessionsToRemember),
    m_previouslyReceivedSessionNumbersUnorderedSet(numReceivedSessionsToRemember + 10), //initial num buckets to prevent rehash
    m_previouslyReceivedSessionNumbersQueueVector(numReceivedSessionsToRemember),
    m_queueIsFull(false),
    m_nextQueueIndex(0)
{

}

LtpSessionRecreationPreventer::~LtpSessionRecreationPreventer() {}

bool LtpSessionRecreationPreventer::AddSession(const uint64_t newSessionNumber) {
    if (m_previouslyReceivedSessionNumbersUnorderedSet.insert(newSessionNumber).second) { //successful insertion
        if (m_queueIsFull) { //remove oldest session number from history
            if (m_previouslyReceivedSessionNumbersUnorderedSet.erase(m_previouslyReceivedSessionNumbersQueueVector[m_nextQueueIndex]) == 0) {
                LOG_ERROR(hdtn::Logger::SubProcess::none) << "LtpSessionRecreationPreventer::AddSession: unable to erase an old value";
                return false;
            }
        }
        m_previouslyReceivedSessionNumbersQueueVector[m_nextQueueIndex++] = newSessionNumber;

        if (m_nextQueueIndex >= M_NUM_RECEIVED_SESSION_NUMBERS_TO_REMEMBER) {
            m_nextQueueIndex = 0;
            m_queueIsFull = true;
        }
        return true;
    }
    return false;
}
bool LtpSessionRecreationPreventer::ContainsSession(const uint64_t newSessionNumber) const {
    return (m_previouslyReceivedSessionNumbersUnorderedSet.count(newSessionNumber) != 0);
}


