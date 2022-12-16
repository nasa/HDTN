#include "Metrics.h"

double Metrics::CalculateMbpsRate(
    double currentBytes,
    double prevBytes, 
    boost::posix_time::ptime nowTime,
    boost::posix_time::ptime lastTime
)
{
    boost::posix_time::time_duration duration = nowTime - lastTime;
    return (8.0 * (currentBytes - prevBytes)) / duration.total_microseconds();
}
