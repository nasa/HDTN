/**
 * @file TokenRateLimiter.cpp
 * @author  Brian Sipos
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

#include "TokenRateLimiter.h"
#include <iostream>




TokenRateLimiter::TokenRateLimiter() : m_rateTokens(0), m_limit(0), m_remain(0) {}

TokenRateLimiter::~TokenRateLimiter() {}

void TokenRateLimiter::SetRate(const int64_t tokens, const boost::posix_time::time_duration & interval, const boost::posix_time::time_duration & window) {
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
    const int64_t delta = m_rateTokens * interval.ticks();
    m_remain += delta;
    if (m_remain > m_limit) {
        m_remain = m_limit;
    }
}

int64_t TokenRateLimiter::GetRemainingTokens() const {
    return m_remain / m_rateInterval.ticks();
}

bool TokenRateLimiter::HasFullBucketOfTokens() const {
    return (m_remain == m_limit);
}

bool TokenRateLimiter::TakeTokens(const uint64_t tokens) {
    if(m_remain < 0) {
        return false;
    }
    const int64_t delta = tokens * m_rateInterval.ticks();
    m_remain -= delta;
    return true;
}

bool TokenRateLimiter::CanTakeTokens() const {
    return (m_remain >= 0);
}
