#ifndef TIMESTAMP_UTIL_H
#define TIMESTAMP_UTIL_H 1

#include <string>
#include <ctime>
#include <boost/date_time.hpp>
#include <ostream>

class TimestampUtil {
private:
    TimestampUtil();
    virtual ~TimestampUtil();

public:

    //Bpv6:
    //All time values in administrative records are UTC times expressed in
    //"DTN time" representation.  A DTN time consists of an SDNV indicating
    //the number of seconds since the start of the year 2000, followed by
    //an SDNV indicating the number of nanoseconds since the start of the
    //indicated second.
    struct dtn_time_t {
        uint64_t secondsSinceStartOfYear2000;
        uint32_t nanosecondsSinceStartOfIndicatedSecond;

        static constexpr unsigned int MAX_BUFFER_SIZE = 18; //MAX(sizeof(__m128i), 10 + sizeof(__m64i)) 

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
        bool DeserializeBpv6(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize);
        void SetZero();
    };

    //Bpv6:
    //Creation Timestamp:   The creation timestamp is a pair of SDNVs that,
    //together with the source endpoint ID and (if the bundle is a
    //fragment) the fragment offset and payload length, serve to
    //identify the bundle.  The first SDNV of the timestamp is the
    //bundle's creation time, while the second is the bundle's creation
    //timestamp sequence number.  Bundle creation time is the time --
    //expressed in seconds since the start of the year 2000, on the
    //Coordinated Universal Time (UTC) scale [UTC] -- at which the
    //transmission request was received that resulted in the creation of
    //the bundle.  Sequence count is the latest value (as of the time at
    //which that transmission request was received) of a monotonically
    //increasing positive integer counter managed by the source node's
    //bundle protocol agent that may be reset to zero whenever the
    //current time advances by one second.  A source Bundle Protocol
    //Agent must never create two distinct bundles with the same source
    //endpoint ID and bundle creation timestamp.  The combination of
    //source endpoint ID and bundle creation timestamp therefore serves
    //to identify a single transmission request, enabling it to be
    //acknowledged by the receiving application (provided the source
    //endpoint ID is not "dtn:none").
    struct bpv6_creation_timestamp_t {
        uint64_t secondsSinceStartOfYear2000;
        uint64_t sequenceNumber;

        bpv6_creation_timestamp_t(); //a default constructor: X()
        bpv6_creation_timestamp_t(uint64_t paramSecondsSinceStartOfYear2000, uint64_t paramSequenceNumber);
        ~bpv6_creation_timestamp_t(); //a destructor: ~X()
        bpv6_creation_timestamp_t(const bpv6_creation_timestamp_t& o); //a copy constructor: X(const X&)
        bpv6_creation_timestamp_t(bpv6_creation_timestamp_t&& o); //a move constructor: X(X&&)
        bpv6_creation_timestamp_t& operator=(const bpv6_creation_timestamp_t& o); //a copy assignment: operator=(const X&)
        bpv6_creation_timestamp_t& operator=(bpv6_creation_timestamp_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const bpv6_creation_timestamp_t & o) const; //operator ==
        bool operator!=(const bpv6_creation_timestamp_t & o) const; //operator !=
        bool operator<(const bpv6_creation_timestamp_t & o) const; //operator < so it can be used as a map key
        friend std::ostream& operator<<(std::ostream& os, const bpv6_creation_timestamp_t& o);
        uint64_t SerializeBpv6(uint8_t * serialization) const;
        uint64_t SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const;
        uint64_t GetSerializationSizeBpv6() const;
        bool DeserializeBpv6(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize);
        void SetZero();
        boost::posix_time::ptime GetPtime() const;
        void SetFromPtime(const boost::posix_time::ptime & posixTimeValue);
        std::string GetUtcTimestampString(bool forFileName) const;
        void SetTimeFromNow();
    };

    //Bpv7:
    //4.2.6. DTN Time
    //A DTN time is an unsigned integer indicating the number of
    //milliseconds that have elapsed since the DTN Epoch, 2000-01-01
    //00:00:00 +0000 (UTC).  DTN time is not affected by leap seconds.
    //Each DTN time SHALL be represented as a CBOR unsigned integer item.
    //Implementers need to be aware that DTN time values conveyed in CBOR
    //representation in bundles will nearly always exceed (2**32 - 1); the
    //manner in which a DTN time value is represented in memory is an
    //implementation matter.  The DTN time value zero indicates that the
    //time is unknown.
    //4.2.7. Creation Timestamp
    //Each bundle's creation timestamp SHALL be represented as a CBOR
    //array comprising two items.
    //The first item of the array, termed "bundle creation time", SHALL be
    //the DTN time at which the transmission request was received that
    //resulted in the creation of the bundle, represented as a CBOR
    //unsigned integer.
    //The second item of the array, termed the creation timestamp's
    //"sequence number", SHALL be the latest value (as of the time at
    //which the transmission request was received) of a monotonically
    //increasing positive integer counter managed by the source node's
    //bundle protocol agent, represented as a CBOR unsigned integer.  The
    //sequence counter MAY be reset to zero whenever the current time
    //advances by one millisecond.
    struct bpv7_creation_timestamp_t {
        uint64_t millisecondsSinceStartOfYear2000;
        uint64_t sequenceNumber;

        static constexpr unsigned int MAX_BUFFER_SIZE = 18; //MAX(sizeof(__m128i), 10 + sizeof(__m64i)) 

        bpv7_creation_timestamp_t(); //a default constructor: X()
        bpv7_creation_timestamp_t(uint64_t paramMillisecondsSinceStartOfYear2000, uint32_t paramSequenceNumber);
        ~bpv7_creation_timestamp_t(); //a destructor: ~X()
        bpv7_creation_timestamp_t(const bpv7_creation_timestamp_t& o); //a copy constructor: X(const X&)
        bpv7_creation_timestamp_t(bpv7_creation_timestamp_t&& o); //a move constructor: X(X&&)
        bpv7_creation_timestamp_t& operator=(const bpv7_creation_timestamp_t& o); //a copy assignment: operator=(const X&)
        bpv7_creation_timestamp_t& operator=(bpv7_creation_timestamp_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const bpv7_creation_timestamp_t & o) const; //operator ==
        bool operator!=(const bpv7_creation_timestamp_t & o) const; //operator !=
        bool operator<(const bpv7_creation_timestamp_t & o) const; //operator < so it can be used as a map key
        friend std::ostream& operator<<(std::ostream& os, const bpv7_creation_timestamp_t& o);
        uint64_t SerializeBpv7(uint8_t * serialization) const;
        uint64_t SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) const;
        uint64_t GetSerializationSize() const;
        bool DeserializeBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize);
        void SetZero();
        boost::posix_time::ptime GetPtime() const;
        void SetFromPtime(const boost::posix_time::ptime & posixTimeValue);
        std::string GetUtcTimestampString(bool forFileName) const;
        void SetTimeFromNow();
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

