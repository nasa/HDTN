/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "StatsLogger.h"

namespace hdtn{

std::unique_ptr<StatsLogger> StatsLogger::StatsLogger_; //initialized to "null"
boost::mutex StatsLogger::mutexSingletonInstance_;
volatile bool StatsLogger::StatsLoggerSingletonFullyInitialized_ = false;
StatsLogger::metric_name_attr_t StatsLogger::metric_name_attr("");
boost::log::sources::logger_mt StatsLogger::logger_;
std::map<std::string, bool> StatsLogger::m_initializedMetrics;

static constexpr uint8_t maxFilesPerMetric = 2;

StatsLogger::StatsLogger()
{
    registerAttributes();
    boost::log::add_common_attributes(); //necessary for timestamp
}

void StatsLogger::ensureInitialized(std::string metricName)
{
    boost::mutex::scoped_lock theLock(mutexSingletonInstance_);
    if (!StatsLoggerSingletonFullyInitialized_) {
        StatsLogger_.reset(new StatsLogger());
        StatsLoggerSingletonFullyInitialized_ = true;
    }
    if (m_initializedMetrics.find(metricName) == m_initializedMetrics.end()) {
        StatsLogger_->createFileSinkForMetric(metricName);
        m_initializedMetrics[metricName] = true;
    }
}

void StatsLogger::registerAttributes()
{
    StatsLogger::logger_.add_attribute("MetricName", StatsLogger::metric_name_attr);
}

int64_t StatsLogger::timestampMs_t::operator()(boost::log::value_ref<boost::posix_time::ptime> const & date) const {
    static const boost::posix_time::ptime start = boost::posix_time::microsec_clock::local_time();
    return (date.get() - start).total_milliseconds();
}

void StatsLogger::createFileSinkForMetric(std::string metricName)
{
    boost::shared_ptr< boost::log::sinks::text_file_backend > backend =
        boost::make_shared< boost::log::sinks::text_file_backend >(
            boost::log::keywords::file_name = "stats/" + metricName + "/" + metricName + "_%Y-%m-%d_%H-%M-%S.csv"
        );

    backend->set_file_collector(boost::log::sinks::file::make_collector(
        boost::log::keywords::max_files = maxFilesPerMetric,
        boost::log::keywords::target = "stats/" + metricName
    ));

    typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_file_backend > sink_t;
    boost::shared_ptr< sink_t > sink = boost::make_shared<sink_t>(backend);
    sink->set_formatter
    (
        boost::log::expressions::stream
            << timestampMsFormatter(boost::log::expressions::attr<boost::posix_time::ptime>("TimeStamp"))
            << "," << boost::log::expressions::smessage
    );
    sink->set_filter(boost::log::expressions::attr<std::string>("MetricName") == metricName);
    sink->locked_backend()->set_open_handler(&StatsLogger::writeHeader);
    sink->locked_backend()->scan_for_files();
    boost::log::core::get()->add_sink(sink);
}

void StatsLogger::writeHeader(boost::log::sinks::text_file_backend::stream_type& file)
{
    file << "timestamp(ms),value\n";
}

StatsLogger::~StatsLogger(){}

}