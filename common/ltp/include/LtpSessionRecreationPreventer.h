#ifndef LTP_SESSION_RECREATION_PREVENTER_H
#define LTP_SESSION_RECREATION_PREVENTER_H 1

#include <cstdint>
#include <vector>
#include <unordered_set>

class LtpSessionRecreationPreventer {
private:
    LtpSessionRecreationPreventer();
public:
    
    LtpSessionRecreationPreventer(const uint64_t numReceivedSessionsToRemember);
    ~LtpSessionRecreationPreventer();
    
    bool AddSession(const uint64_t newSessionNumber);
    bool ContainsSession(const uint64_t newSessionNumber) const;

private:
    const uint64_t M_NUM_RECEIVED_SESSION_NUMBERS_TO_REMEMBER;
    std::unordered_set<uint64_t> m_previouslyReceivedSessionNumbersUnorderedSet;
    std::vector<uint64_t> m_previouslyReceivedSessionNumbersQueueVector;
    bool m_queueIsFull;
    uint64_t m_nextQueueIndex;
};

#endif // LTP_SESSION_RECREATION_PREVENTER_H

