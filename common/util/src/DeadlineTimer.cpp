#include <boost/make_unique.hpp>

#include "DeadlineTimer.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

DeadlineTimer::DeadlineTimer(unsigned int intervalMs)
    : m_ioService(),
    m_sleepValTimeDuration(boost::posix_time::milliseconds(intervalMs)),
    m_deadlineTimer(m_ioService, m_sleepValTimeDuration),
    m_enabled(true)
{
}

bool DeadlineTimer::SleepUntilNextInterval() {
    if (!m_enabled) {
        return false;
    }
    boost::system::error_code ec;
    m_deadlineTimer.wait(ec);
    if (ec) {
        LOG_FATAL(subprocess) << "timer error: " << ec.message();
        return false;
    }
    m_deadlineTimer.expires_at(m_deadlineTimer.expires_at() + m_sleepValTimeDuration);
    return true;
}

void DeadlineTimer::Cancel() {
    try {
        m_deadlineTimer.cancel();
    }
    catch (const boost::system::system_error& e) {
        LOG_ERROR(subprocess) << "DeadlineTimer::Cancel() had error: " << e.what();
    }
}
void DeadlineTimer::Disable() noexcept {
    m_enabled = false;
}
