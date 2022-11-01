/**
 * @file LtpSessionRecreationPreventer.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "LtpSessionRecreationPreventer.h"
#include "Logger.h"

LtpSessionRecreationPreventer::LtpSessionRecreationPreventer(const uint64_t numReceivedSessionsToRemember) :
    M_NUM_RECEIVED_SESSION_NUMBERS_TO_REMEMBER(numReceivedSessionsToRemember),
    m_previouslyReceivedSessionNumbersUnorderedSet(numReceivedSessionsToRemember + 10), //intial num buckets to prevent rehash
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


