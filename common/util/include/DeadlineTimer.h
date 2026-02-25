/**
 * @file DeadlineTimer.h
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
 * This DeadlineTimer implements sleeping for an amount of time to maintain a precise
 * run interval
 */

#ifndef _DEADLINE_TIMER_H
#define _DEADLINE_TIMER_H 1

#include <boost/asio.hpp>
#include <atomic>
#include "hdtn_util_export.h"

class DeadlineTimer
{
    public:
        HDTN_UTIL_EXPORT DeadlineTimer(unsigned int intervalMs);
        HDTN_UTIL_EXPORT bool SleepUntilNextInterval();
        HDTN_UTIL_EXPORT void Cancel();
        HDTN_UTIL_EXPORT void Disable() noexcept;

    private:
        DeadlineTimer() = delete;
        boost::asio::io_service m_ioService;
        boost::posix_time::time_duration m_sleepValTimeDuration;
        boost::asio::deadline_timer m_deadlineTimer;
        std::atomic<bool> m_enabled;
};

#endif // _DEADLINE_TIMER_H
