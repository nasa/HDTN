#include <boost/make_unique.hpp>

#include "DeadlineTimer.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

DeadlineTimer::DeadlineTimer(unsigned int sleepMs)
{
    boost::asio::io_service ioService;
    m_sleepValTimeDuration = boost::posix_time::milliseconds(sleepMs);
    m_deadlineTimer = boost::make_unique<boost::asio::deadline_timer>(ioService, m_sleepValTimeDuration);
}

bool DeadlineTimer::Sleep()
{
    boost::system::error_code ec;
    m_deadlineTimer->wait(ec);
    if (ec) {
        LOG_FATAL(subprocess) << "timer error: " << ec.message();
        return false;
    }
    m_deadlineTimer->expires_at(m_deadlineTimer->expires_at() + m_sleepValTimeDuration);
    return true;
}
