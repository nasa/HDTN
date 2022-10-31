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

StatsLogger::StatsLogger()
{
    registerAttributes();
    createMultiFileLogSinkForMetricNameAttr();
    boost::log::add_common_attributes(); //necessary for timestamp
}

void StatsLogger::ensureInitialized()
{
    if (!StatsLoggerSingletonFullyInitialized_) { //fast way to bypass a mutex lock all the time
        //first thread that uses the stats writer gets to create the stats writer
        boost::mutex::scoped_lock theLock(mutexSingletonInstance_);
        if (!StatsLoggerSingletonFullyInitialized_) { //check it again now that mutex is locked
            StatsLogger_.reset(new StatsLogger());
            StatsLoggerSingletonFullyInitialized_ = true;
        }
    }
}

void StatsLogger::registerAttributes()
{
    StatsLogger::logger_.add_attribute("MetricName", StatsLogger::metric_name_attr);
}

long StatsLogger::millisecondsSinceEpoch_t::operator()(boost::log::value_ref<boost::posix_time::ptime> const & date) const {
    static boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
    return (date.get() - epoch).total_milliseconds();
}

void StatsLogger::createMultiFileLogSinkForMetricNameAttr()
{
    boost::shared_ptr< boost::log::sinks::text_multifile_backend > backend =
        boost::make_shared< boost::log::sinks::text_multifile_backend >();
    
    backend->set_file_name_composer
    (
        boost::log::sinks::file::as_file_name_composer(
            boost::log::expressions::stream << "stats/" <<
                boost::log::expressions::attr< std::string > ("MetricName") << ".csv"
        )
    );

    typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_multifile_backend > sink_t;
    boost::shared_ptr< sink_t > sink = boost::make_shared<sink_t>(backend);
    sink->set_formatter
    (
        boost::log::expressions::stream
            << boost::log::expressions::attr< std::string >("MetricName")
            << "," << millisecondsSinceEpochFormatter(boost::log::expressions::attr<boost::posix_time::ptime>("TimeStamp"))
            << "," << boost::log::expressions::smessage
    );
    boost::log::core::get()->add_sink(sink);
}

StatsLogger::~StatsLogger(){}

}