#ifndef TIMESTAMP_UTIL_H
#define TIMESTAMP_UTIL_H 1

#include <string>
#include <ctime>
#include <boost/date_time.hpp>

class TimestampUtil {
private:
	TimestampUtil();
	virtual ~TimestampUtil();
	
public:
	static const boost::posix_time::ptime & GetRfc5050Epoch();
	static const boost::posix_time::ptime & GetUnixEpoch();

	static uint64_t GetMillisecondsSinceEpochUnix();
	static uint64_t GetMillisecondsSinceEpochUnix(const boost::posix_time::ptime & posixTimeValue);
    static uint64_t GetMillisecondsSinceEpochRfc5050();
    static uint64_t GetMillisecondsSinceEpochRfc5050(const boost::posix_time::ptime & posixTimeValue);
    static uint64_t GetMillisecondsSinceEpoch(const boost::posix_time::ptime & posixTimeValue, const boost::posix_time::ptime & epochStartTime);

    static uint64_t GetSecondsSinceEpochUnix();
    static uint64_t GetSecondsSinceEpochUnix(const boost::posix_time::ptime & posixTimeValue);
    static uint64_t GetSecondsSinceEpochRfc5050();
    static uint64_t GetSecondsSinceEpochRfc5050(const boost::posix_time::ptime & posixTimeValue);
    static uint64_t GetSecondsSinceEpoch(const boost::posix_time::ptime & posixTimeValue, const boost::posix_time::ptime & epochStartTime);

    static uint64_t GetMicrosecondsSinceEpochUnix();
    static uint64_t GetMicrosecondsSinceEpochUnix(const boost::posix_time::ptime & posixTimeValue);
    static uint64_t GetMicrosecondsSinceEpochRfc5050();
    static uint64_t GetMicrosecondsSinceEpochRfc5050(const boost::posix_time::ptime & posixTimeValue);
    static uint64_t GetMicrosecondsSinceEpoch(const boost::posix_time::ptime & posixTimeValue, const boost::posix_time::ptime & epochStartTime);

	static std::string GetUtcTimestampStringNow(bool forFileName);
	static std::string GetUtcTimestampStringFromPtime(const boost::posix_time::ptime & posixTimeValue, bool forFileName);
	static bool SetPtimeFromUtcTimestampString(const std::string & stringvalue, boost::posix_time::ptime & pt);

};

#endif //TIMESTAMP_UTIL_H

