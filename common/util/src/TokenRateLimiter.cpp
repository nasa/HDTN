#include "TokenRateLimiter.h"
#include <iostream>

TokenRateLimiter::TokenRateLimiter() : m_rateTokens(0), m_limit(0), m_remain(0) {}

TokenRateLimiter::~TokenRateLimiter() {}

void TokenRateLimiter::SetRate(const uint64_t tokens, const boost::posix_time::time_duration & interval, const boost::posix_time::time_duration & window) {
    if (interval.is_special()) {
        return;

    }
    m_rateTokens = tokens;
    m_rateInterval = interval;

    m_limit = m_rateTokens * window.ticks();
    m_remain = m_limit;
}

void TokenRateLimiter::AddTime(const boost::posix_time::time_duration & interval) {
    if (interval.is_special()) {
        return;
    }
    const uint64_t delta = m_rateTokens * interval.ticks();
    m_remain += delta;
    if (m_remain > m_limit) {
        m_remain = m_limit;
    }
}

uint64_t TokenRateLimiter::GetRemainingTokens() const {
    return m_remain / m_rateInterval.ticks();
}

bool TokenRateLimiter::HasFullBucketOfTokens() const {
    return (m_remain == m_limit);
}

bool TokenRateLimiter::TakeTokens(const uint64_t tokens) {
    const uint64_t delta = tokens * m_rateInterval.ticks();
    if (delta > m_remain) {
        return false;
    }
    m_remain -= delta;
    return true;
}
