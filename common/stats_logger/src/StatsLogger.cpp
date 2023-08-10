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
std::atomic<bool> StatsLogger::StatsLoggerSingletonFullyInitialized_ = false;
StatsLogger::file_name_attr_t StatsLogger::file_name_attr("");
boost::log::sources::logger_mt StatsLogger::m_logger;
std::map<std::string, boost::shared_ptr<StatsLogger::sink_t>> StatsLogger::m_initializedFiles;

static constexpr uint8_t maxFilesPerMetric = 2;

/**
 * Overloads the stream operator to support metric_t
 */
std::ostream& operator<< (std::ostream& strm, const StatsLogger::metric_t m)
{
    if (m.isFloat) {
        const auto default_precision {std::cout.precision()};
        strm << std::fixed << std::setprecision(2) << m.floatval << std::setprecision(default_precision);
        strm.unsetf(std::ios_base::fixed);
        return strm;
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

void StatsLogger::Log(
    const std::string& fileName,
    const std::vector<StatsLogger::metric_t>& metrics
)
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

void StatsLogger::Reset()
{
    boost::mutex::scoped_lock theLock(mutexSingletonInstance_);
   
    std::map<std::string, boost::shared_ptr<StatsLogger::sink_t>>::iterator it;
    for (it = m_initializedFiles.begin(); it != m_initializedFiles.end(); it++) {
        boost::log::core::get()->remove_sink(it->second);
    }
    m_initializedFiles.clear();
}

void StatsLogger::ensureInitialized(
    const std::string& fileName,
    const std::vector<StatsLogger::metric_t>& metrics
)
{
    boost::mutex::scoped_lock theLock(mutexSingletonInstance_);
    if (!StatsLoggerSingletonFullyInitialized_) {
        StatsLogger_.reset(new StatsLogger());
        StatsLoggerSingletonFullyInitialized_ = true;
    }
    if (m_initializedFiles.find(fileName) == m_initializedFiles.end()) {
        boost::shared_ptr< StatsLogger::sink_t > sink = StatsLogger_->createFileSink(fileName, metrics);
        m_initializedFiles[fileName] = sink;
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

 boost::shared_ptr< StatsLogger::sink_t > StatsLogger::createFileSink(
    const std::string& fileName,
    const std::vector<StatsLogger::metric_t>& metrics
)
{
    boost::shared_ptr< boost::log::sinks::text_file_backend > backend =
        boost::make_shared< boost::log::sinks::text_file_backend >(
            boost::log::keywords::file_name = "stats/" + fileName + "/" + fileName + "_%Y-%m-%d_%H-%M-%S.csv"
        );

    backend->set_file_collector(boost::log::sinks::file::make_collector(
        boost::log::keywords::max_files = maxFilesPerMetric,
        boost::log::keywords::target = "stats/" + fileName
    ));

    boost::shared_ptr< sink_t > sink = boost::make_shared<sink_t>(backend);
    sink->set_filter(boost::log::expressions::attr<std::string>("fileName") == fileName);
    sink->locked_backend()->scan_for_files();
    sink->locked_backend()->auto_flush(true);
    boost::log::core::get()->add_sink(sink);
    StatsLogger::writeHeader(fileName, metrics);

    sink->set_formatter
    (
        boost::log::expressions::stream
            << m_timestampMsFormatter(boost::log::expressions::attr<boost::posix_time::ptime>("TimeStamp"))
            << "," << boost::log::expressions::smessage
    );
    return sink;
}

void StatsLogger::writeHeader(
    const std::string& fileName,
    const std::vector<StatsLogger::metric_t>& metrics
)
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