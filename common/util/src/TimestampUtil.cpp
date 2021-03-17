#include <sstream>
#include "TimestampUtil.h"

static const boost::posix_time::ptime EPOCH_START_TIME_UNIX(boost::gregorian::date(1970, 1, 1));

//J2000 starts at January 1, 2000, 11:58:55.816 UTC
//static const boost::posix_time::ptime EPOCH_START_TIME_J2000(boost::gregorian::date(2000, 1, 1), boost::posix_time::time_duration(11, 58, 55) + boost::posix_time::milliseconds(816));


static const boost::posix_time::ptime EPOCH_START_TIME_RFC5050(boost::gregorian::date(2000, 1, 1));


const boost::posix_time::ptime & TimestampUtil::GetRfc5050Epoch() {
	return EPOCH_START_TIME_RFC5050;
}
const boost::posix_time::ptime & TimestampUtil::GetUnixEpoch() {
	return EPOCH_START_TIME_UNIX;
}

uint64_t TimestampUtil::GetMillisecondsSinceEpochUnix() {
    return GetMillisecondsSinceEpoch(boost::posix_time::microsec_clock::universal_time(), EPOCH_START_TIME_UNIX);
}
uint64_t TimestampUtil::GetMillisecondsSinceEpochUnix(const boost::posix_time::ptime & posixTimeValue) {
    return GetMillisecondsSinceEpoch(posixTimeValue, EPOCH_START_TIME_UNIX);
}

uint64_t TimestampUtil::GetMillisecondsSinceEpochRfc5050() {
    return GetMillisecondsSinceEpoch(boost::posix_time::microsec_clock::universal_time(), EPOCH_START_TIME_RFC5050);
}
uint64_t TimestampUtil::GetMillisecondsSinceEpochRfc5050(const boost::posix_time::ptime & posixTimeValue) {
    return GetMillisecondsSinceEpoch(posixTimeValue, EPOCH_START_TIME_RFC5050);
}
uint64_t TimestampUtil::GetMillisecondsSinceEpoch(const boost::posix_time::ptime & posixTimeValue, const boost::posix_time::ptime & epochStartTime) {
    const boost::posix_time::time_duration diff = posixTimeValue - epochStartTime;
    const boost::int64_t milliSecondsNormalized = diff.total_milliseconds();
    return static_cast<uint64_t>(milliSecondsNormalized);
}

uint64_t TimestampUtil::GetSecondsSinceEpochUnix() {
    return GetSecondsSinceEpoch(boost::posix_time::microsec_clock::universal_time(), EPOCH_START_TIME_UNIX);
}
uint64_t TimestampUtil::GetSecondsSinceEpochUnix(const boost::posix_time::ptime & posixTimeValue) {
    return GetSecondsSinceEpoch(posixTimeValue, EPOCH_START_TIME_UNIX);
}

uint64_t TimestampUtil::GetSecondsSinceEpochRfc5050() {
    return GetSecondsSinceEpoch(boost::posix_time::microsec_clock::universal_time(), EPOCH_START_TIME_RFC5050);
}
uint64_t TimestampUtil::GetSecondsSinceEpochRfc5050(const boost::posix_time::ptime & posixTimeValue) {
    return GetSecondsSinceEpoch(posixTimeValue, EPOCH_START_TIME_RFC5050);
}
uint64_t TimestampUtil::GetSecondsSinceEpoch(const boost::posix_time::ptime & posixTimeValue, const boost::posix_time::ptime & epochStartTime) {
    const boost::posix_time::time_duration diff = posixTimeValue - epochStartTime;
    const boost::int64_t secondsNormalized = diff.total_seconds();
    return static_cast<uint64_t>(secondsNormalized);
}


uint64_t TimestampUtil::GetMicrosecondsSinceEpochUnix() {
    return GetMicrosecondsSinceEpoch(boost::posix_time::microsec_clock::universal_time(), EPOCH_START_TIME_UNIX);
}
uint64_t TimestampUtil::GetMicrosecondsSinceEpochUnix(const boost::posix_time::ptime & posixTimeValue) {
    return GetMicrosecondsSinceEpoch(posixTimeValue, EPOCH_START_TIME_UNIX);
}

uint64_t TimestampUtil::GetMicrosecondsSinceEpochRfc5050() {
    return GetMicrosecondsSinceEpoch(boost::posix_time::microsec_clock::universal_time(), EPOCH_START_TIME_RFC5050);
}
uint64_t TimestampUtil::GetMicrosecondsSinceEpochRfc5050(const boost::posix_time::ptime & posixTimeValue) {
    return GetMicrosecondsSinceEpoch(posixTimeValue, EPOCH_START_TIME_RFC5050);
}
uint64_t TimestampUtil::GetMicrosecondsSinceEpoch(const boost::posix_time::ptime & posixTimeValue, const boost::posix_time::ptime & epochStartTime) {
    const boost::posix_time::time_duration diff = posixTimeValue - epochStartTime;
    const boost::int64_t microsecondsNormalized = diff.total_microseconds();
    return static_cast<uint64_t>(microsecondsNormalized);
}


std::string TimestampUtil::GetUtcTimestampStringNow(bool forFileName) {
	return GetUtcTimestampStringFromPtime(boost::posix_time::microsec_clock::universal_time(), forFileName);
}

// %Y = Four digit year
// %m = Month name as a decimal 01 to 12
// %d = Day of the month as decimal 01 to 31
// %H = The hour as a decimal number using a 24 - hour clock(range 00 to 23).
// %M = The minute as a decimal number(range 00 to 59).
// %s = Seconds with fractional seconds.
// %f fractional seconds ".123456"
//"timestamp": "2018-02-28T09:43:41.688",
//strftime(cBuffer, 60, "%Y-%m-%dT%H:%M:%S", ltime);
//sprintf(cBuffer, "%s.%03d", cBuffer, (int)_t.tv_usec / 1000);



std::string TimestampUtil::GetUtcTimestampStringFromPtime(const boost::posix_time::ptime & posixTimeValue, bool forFileName) {
	static const std::locale DATE_TIME_FORMAT_OUT_UTC = std::locale(std::locale::classic(), new boost::posix_time::time_facet("%Y-%m-%dT%H:%M:%sZ"));
	static const std::locale DATE_TIME_FORMAT_OUT_UTC_FILENAME = std::locale(std::locale::classic(), new boost::posix_time::time_facet("%Y_%m_%dT%H_%M_%sZ"));
	std::ostringstream os;
	os.imbue((forFileName) ? DATE_TIME_FORMAT_OUT_UTC_FILENAME : DATE_TIME_FORMAT_OUT_UTC);
	os << posixTimeValue;
	return os.str();
}


bool TimestampUtil::SetPtimeFromUtcTimestampString(const std::string & stringvalue, boost::posix_time::ptime & pt) {
	//"2020-02-06T20:25:11.493Z"
	static const std::locale DATE_TIME_FORMAT_IN = std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%sZ"));

	std::istringstream is(stringvalue);
	is.imbue(DATE_TIME_FORMAT_IN);
	is >> pt;
	if (pt.is_not_a_date_time()) {
		return false;
	}
	return true;

}
