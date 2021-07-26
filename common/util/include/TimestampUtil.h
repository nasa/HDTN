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

    //All time values in administrative records are UTC times expressed in
    //"DTN time" representation.  A DTN time consists of an SDNV indicating
    //the number of seconds since the start of the year 2000, followed by
    //an SDNV indicating the number of nanoseconds since the start of the
    //indicated second.
    struct dtn_time_t {
        uint64_t secondsSinceStartOfYear2000;
        uint32_t nanosecondsSinceStartOfIndicatedSecond;

        dtn_time_t(); //a default constructor: X()
        dtn_time_t(uint64_t paramSecondsSinceStartOfYear2000, uint32_t paramNanosecondsSinceStartOfIndicatedSecond);
        ~dtn_time_t(); //a destructor: ~X()
        dtn_time_t(const dtn_time_t& o); //a copy constructor: X(const X&)
        dtn_time_t(dtn_time_t&& o); //a move constructor: X(X&&)
        dtn_time_t& operator=(const dtn_time_t& o); //a copy assignment: operator=(const X&)
        dtn_time_t& operator=(dtn_time_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const dtn_time_t & o) const; //operator ==
        bool operator!=(const dtn_time_t & o) const; //operator !=
        bool operator<(const dtn_time_t & o) const; //operator < so it can be used as a map key
        friend std::ostream& operator<<(std::ostream& os, const dtn_time_t& o);
        uint64_t Serialize(uint8_t * serialization) const;
        void SetZero();
    };

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

    static boost::posix_time::ptime DtnTimeToPtimeLossy(const TimestampUtil::dtn_time_t & dtnTime);
    static TimestampUtil::dtn_time_t PtimeToDtnTime(const boost::posix_time::ptime & posixTimeValue);
    static std::string GetUtcTimestampStringFromDtnTimeLossy(const TimestampUtil::dtn_time_t & dtnTime, bool forFileName);
    static TimestampUtil::dtn_time_t GenerateDtnTimeNow();
};

#endif //TIMESTAMP_UTIL_H

