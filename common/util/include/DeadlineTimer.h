/**
 * @file DeadlineTimer.h
 *
 * @copyright Copyright Ⓒ 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 * This DeadlineTimer class provides a wrapper around the Boost deadline_timer
 */

#ifndef _DEADLINE_TIMER_H
#define _DEADLINE_TIMER_H 1

#include <boost/asio.hpp>

#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT DeadlineTimer
{
    public:
        DeadlineTimer(unsigned int sleepMs);
        bool Sleep();

    private:
        DeadlineTimer();
        std::unique_ptr<boost::asio::deadline_timer> m_deadlineTimer;
        boost::posix_time::time_duration m_sleepValTimeDuration;
};

#endif // _DEADLINE_TIMER_H
