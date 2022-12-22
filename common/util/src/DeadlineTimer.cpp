#include <boost/make_unique.hpp>

#include "DeadlineTimer.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

DeadlineTimer::DeadlineTimer(unsigned int intervalMs)
    : m_ioService(),
    m_sleepValTimeDuration(boost::posix_time::milliseconds(intervalMs)),
    m_deadlineTimer(m_ioService, m_sleepValTimeDuration)
{
}

bool DeadlineTimer::SleepUntilNextInterval()
{
    boost::system::error_code ec;
    m_deadlineTimer.wait(ec);
    if (ec) {
        LOG_FATAL(subprocess) << "timer error: " << ec.message();
        return false;
    }
    m_deadlineTimer.expires_at(m_deadlineTimer.expires_at() + m_sleepValTimeDuration);
    return true;
}
