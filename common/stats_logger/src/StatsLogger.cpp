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
StatsLogger::file_name_attr_t StatsLogger::file_name_attr("");
boost::log::sources::logger_mt StatsLogger::m_logger;
std::map<std::string, bool> StatsLogger::m_initializedFiles;

static constexpr uint8_t maxFilesPerMetric = 2;

/**
 * Overloads the stream operator to support metric_t
 */
std::ostream& operator<< (std::ostream& strm, const StatsLogger::metric_t m)
{
    if (m.isFloat) {
        const auto default_precision {std::cout.precision()};
        return strm << std::fixed << std::setprecision(2) << m.floatval << std::setprecision(default_precision);
    }
    return strm << m.intval;
}

StatsLogger::metric_t::metric_t(std::string name, uint64_t val)
    : isFloat(false), name(name), floatval(0), intval(val)
{}

StatsLogger::metric_t::metric_t(std::string name, double val)
    : isFloat(true), name(name), floatval(val), intval(0)
{}

StatsLogger::StatsLogger()
{
    registerAttributes();
    boost::log::add_common_attributes(); //necessary for timestamp
}

void StatsLogger::Log(std::string fileName, std::vector<StatsLogger::metric_t> metrics)
{
    hdtn::StatsLogger::ensureInitialized(fileName, metrics);

    std::stringstream ss;
    for (int i = 0; i < metrics.size(); i++) {
        ss << metrics[i];
        if (i < metrics.size() - 1) {
            ss << ",";
        }
    }

    boost::mutex::scoped_lock theLock(mutexSingletonInstance_);
    hdtn::StatsLogger::file_name_attr.set(fileName);
    BOOST_LOG(hdtn::StatsLogger::m_logger) << ss.str();
}

void StatsLogger::ensureInitialized(std::string fileName, std::vector<StatsLogger::metric_t> metrics)
{
    boost::mutex::scoped_lock theLock(mutexSingletonInstance_);
    if (!StatsLoggerSingletonFullyInitialized_) {
        StatsLogger_.reset(new StatsLogger());
        StatsLoggerSingletonFullyInitialized_ = true;
    }
    if (m_initializedFiles.find(fileName) == m_initializedFiles.end()) {
        StatsLogger_->createFileSink(fileName, metrics);
        m_initializedFiles[fileName] = true;
    }
}

void StatsLogger::registerAttributes()
{
    StatsLogger::m_logger.add_attribute("fileName", StatsLogger::file_name_attr);
}

int64_t StatsLogger::timestampMs_t::operator()(boost::log::value_ref<boost::posix_time::ptime> const & date) const {
    static const boost::posix_time::ptime start = boost::posix_time::microsec_clock::local_time();
    return (date.get() - start).total_milliseconds();
}

void StatsLogger::createFileSink(std::string fileName, std::vector<StatsLogger::metric_t> metrics)
{
    boost::shared_ptr< boost::log::sinks::text_file_backend > backend =
        boost::make_shared< boost::log::sinks::text_file_backend >(
            boost::log::keywords::file_name = "stats/" + fileName + "/" + fileName + "_%Y-%m-%d_%H-%M-%S.csv"
        );

    backend->set_file_collector(boost::log::sinks::file::make_collector(
        boost::log::keywords::max_files = maxFilesPerMetric,
        boost::log::keywords::target = "stats/" + fileName
    ));

    typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_file_backend > sink_t;
    boost::shared_ptr< sink_t > sink = boost::make_shared<sink_t>(backend);
    sink->set_filter(boost::log::expressions::attr<std::string>("fileName") == fileName);
    sink->locked_backend()->scan_for_files();
    boost::log::core::get()->add_sink(sink);
    StatsLogger::writeHeader(fileName, metrics);

    sink->set_formatter
    (
        boost::log::expressions::stream
            << timestampMsFormatter(boost::log::expressions::attr<boost::posix_time::ptime>("TimeStamp"))
            << "," << boost::log::expressions::smessage
    );
}

void StatsLogger::writeHeader(std::string fileName, std::vector<StatsLogger::metric_t> metrics)
{
    std::stringstream ss;
    for (int i = 0; i < metrics.size(); i++) {
        ss << metrics[i].name;
        if (i < metrics.size() - 1) {
            ss << ",";
        }
    }
    hdtn::StatsLogger::file_name_attr.set(fileName);
    BOOST_LOG(hdtn::StatsLogger::m_logger) << "timestamp(ms)," << ss.str();
}

StatsLogger::~StatsLogger(){}

}