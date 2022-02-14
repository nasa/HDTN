#include <sstream>
#include "TimestampUtil.h"
#include "Sdnv.h"
#include "CborUint.h"

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


TimestampUtil::dtn_time_t::dtn_time_t() : secondsSinceStartOfYear2000(0), nanosecondsSinceStartOfIndicatedSecond(0) { } //a default constructor: X()
TimestampUtil::dtn_time_t::dtn_time_t(uint64_t paramSecondsSinceStartOfYear2000, uint32_t paramNanosecondsSinceStartOfIndicatedSecond) :
    secondsSinceStartOfYear2000(paramSecondsSinceStartOfYear2000), nanosecondsSinceStartOfIndicatedSecond(paramNanosecondsSinceStartOfIndicatedSecond) { }
TimestampUtil::dtn_time_t::~dtn_time_t() { } //a destructor: ~X()
TimestampUtil::dtn_time_t::dtn_time_t(const dtn_time_t& o) : secondsSinceStartOfYear2000(o.secondsSinceStartOfYear2000), nanosecondsSinceStartOfIndicatedSecond(o.nanosecondsSinceStartOfIndicatedSecond) { } //a copy constructor: X(const X&)
TimestampUtil::dtn_time_t::dtn_time_t(dtn_time_t&& o) : secondsSinceStartOfYear2000(o.secondsSinceStartOfYear2000), nanosecondsSinceStartOfIndicatedSecond(o.nanosecondsSinceStartOfIndicatedSecond) { } //a move constructor: X(X&&)
TimestampUtil::dtn_time_t& TimestampUtil::dtn_time_t::operator=(const dtn_time_t& o) { //a copy assignment: operator=(const X&)
    secondsSinceStartOfYear2000 = o.secondsSinceStartOfYear2000;
    nanosecondsSinceStartOfIndicatedSecond = o.nanosecondsSinceStartOfIndicatedSecond;
    return *this;
}
TimestampUtil::dtn_time_t& TimestampUtil::dtn_time_t::operator=(dtn_time_t && o) { //a move assignment: operator=(X&&)
    secondsSinceStartOfYear2000 = o.secondsSinceStartOfYear2000;
    nanosecondsSinceStartOfIndicatedSecond = o.nanosecondsSinceStartOfIndicatedSecond;
    return *this;
}
bool TimestampUtil::dtn_time_t::operator==(const dtn_time_t & o) const {
    return (secondsSinceStartOfYear2000 == o.secondsSinceStartOfYear2000) && (nanosecondsSinceStartOfIndicatedSecond == o.nanosecondsSinceStartOfIndicatedSecond);
}
bool TimestampUtil::dtn_time_t::operator!=(const dtn_time_t & o) const {
    return (secondsSinceStartOfYear2000 != o.secondsSinceStartOfYear2000) || (nanosecondsSinceStartOfIndicatedSecond != o.nanosecondsSinceStartOfIndicatedSecond);
}
bool TimestampUtil::dtn_time_t::operator<(const dtn_time_t & o) const {
    if (secondsSinceStartOfYear2000 == o.secondsSinceStartOfYear2000) {
        return (nanosecondsSinceStartOfIndicatedSecond < o.nanosecondsSinceStartOfIndicatedSecond);
    }
    return (secondsSinceStartOfYear2000 < o.secondsSinceStartOfYear2000);
}
std::ostream& operator<<(std::ostream& os, const TimestampUtil::dtn_time_t & o) {
    os << "secondsSinceStartOfYear2000: " << o.secondsSinceStartOfYear2000 << ", nanosecondsSinceStartOfIndicatedSecond: " << o.nanosecondsSinceStartOfIndicatedSecond;
    return os;
}
uint64_t TimestampUtil::dtn_time_t::SerializeBpv6(uint8_t * serialization) const {
    uint8_t * serializationBase = serialization;
    serialization += SdnvEncodeU64BufSize10(serialization, secondsSinceStartOfYear2000);
    serialization += SdnvEncodeU32BufSize8(serialization, nanosecondsSinceStartOfIndicatedSecond);
    return serialization - serializationBase;
}
uint64_t TimestampUtil::dtn_time_t::SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const {
    uint8_t * serializationBase = serialization;
    uint64_t thisSerializationSize;

    thisSerializationSize = SdnvEncodeU64(serialization, secondsSinceStartOfYear2000, bufferSize); //if zero returned on failure will only malform the bundle but not cause memory overflow
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    thisSerializationSize = SdnvEncodeU32(serialization, nanosecondsSinceStartOfIndicatedSecond, bufferSize);
    serialization += thisSerializationSize;
    //bufferSize -= thisSerializationSize; //not needed

    return serialization - serializationBase;
}
uint64_t TimestampUtil::dtn_time_t::GetSerializationSizeBpv6() const {
    return SdnvGetNumBytesRequiredToEncode(secondsSinceStartOfYear2000) + SdnvGetNumBytesRequiredToEncode(nanosecondsSinceStartOfIndicatedSecond);
}
bool TimestampUtil::dtn_time_t::DeserializeBpv6(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;

    secondsSinceStartOfYear2000 = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    nanosecondsSinceStartOfIndicatedSecond = SdnvDecodeU32(serialization, &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return false; //failure
    }
    serialization += sdnvSize;
    //bufferSize -= sdnvSize; //not needed

    *numBytesTakenToDecode = static_cast<uint8_t>(serialization - serializationBase);
    return true;
}
void TimestampUtil::dtn_time_t::SetZero() {
    secondsSinceStartOfYear2000 = 0;
    nanosecondsSinceStartOfIndicatedSecond = 0;
}

boost::posix_time::ptime TimestampUtil::DtnTimeToPtimeLossy(const TimestampUtil::dtn_time_t & dtnTime) {
    //Note the returned ptime will lose the nanosecond precision and be truncated to microseconds
    return TimestampUtil::GetRfc5050Epoch() + boost::posix_time::seconds(dtnTime.secondsSinceStartOfYear2000) +
#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
        boost::posix_time::nanoseconds(dtnTime.nanosecondsSinceStartOfIndicatedSecond);
#else
        boost::posix_time::microseconds(dtnTime.nanosecondsSinceStartOfIndicatedSecond / 1000);
#endif // BOOST_DATE_TIME_HAS_NANOSECONDS
}

TimestampUtil::dtn_time_t TimestampUtil::PtimeToDtnTime(const boost::posix_time::ptime & posixTimeValue) {
    const boost::posix_time::time_duration diff = posixTimeValue - TimestampUtil::GetRfc5050Epoch();
    return TimestampUtil::dtn_time_t(
        static_cast<uint64_t>(diff.total_seconds()),
        static_cast<uint32_t>(diff.fractional_seconds() * (1000000000 / boost::posix_time::time_res_traits::ticks_per_second)) //convert to nanoseconds
    );
}


std::string TimestampUtil::GetUtcTimestampStringFromDtnTimeLossy(const TimestampUtil::dtn_time_t & dtnTime, bool forFileName) {
    return TimestampUtil::GetUtcTimestampStringFromPtime(DtnTimeToPtimeLossy(dtnTime), forFileName);
}

TimestampUtil::dtn_time_t TimestampUtil::GenerateDtnTimeNow() {
    return PtimeToDtnTime(boost::posix_time::microsec_clock::universal_time());
}

//Bpv6
TimestampUtil::bpv6_creation_timestamp_t::bpv6_creation_timestamp_t() : secondsSinceStartOfYear2000(0), sequenceNumber(0) { } //a default constructor: X()
TimestampUtil::bpv6_creation_timestamp_t::bpv6_creation_timestamp_t(uint64_t paramSecondsSinceStartOfYear2000, uint64_t paramSequenceNumber) :
    secondsSinceStartOfYear2000(paramSecondsSinceStartOfYear2000), sequenceNumber(paramSequenceNumber) { }
TimestampUtil::bpv6_creation_timestamp_t::~bpv6_creation_timestamp_t() { } //a destructor: ~X()
TimestampUtil::bpv6_creation_timestamp_t::bpv6_creation_timestamp_t(const bpv6_creation_timestamp_t& o) : secondsSinceStartOfYear2000(o.secondsSinceStartOfYear2000), sequenceNumber(o.sequenceNumber) { } //a copy constructor: X(const X&)
TimestampUtil::bpv6_creation_timestamp_t::bpv6_creation_timestamp_t(bpv6_creation_timestamp_t&& o) : secondsSinceStartOfYear2000(o.secondsSinceStartOfYear2000), sequenceNumber(o.sequenceNumber) { } //a move constructor: X(X&&)
TimestampUtil::bpv6_creation_timestamp_t& TimestampUtil::bpv6_creation_timestamp_t::operator=(const bpv6_creation_timestamp_t& o) { //a copy assignment: operator=(const X&)
    secondsSinceStartOfYear2000 = o.secondsSinceStartOfYear2000;
    sequenceNumber = o.sequenceNumber;
    return *this;
}
TimestampUtil::bpv6_creation_timestamp_t& TimestampUtil::bpv6_creation_timestamp_t::operator=(bpv6_creation_timestamp_t && o) { //a move assignment: operator=(X&&)
    secondsSinceStartOfYear2000 = o.secondsSinceStartOfYear2000;
    sequenceNumber = o.sequenceNumber;
    return *this;
}
bool TimestampUtil::bpv6_creation_timestamp_t::operator==(const bpv6_creation_timestamp_t & o) const {
    return (secondsSinceStartOfYear2000 == o.secondsSinceStartOfYear2000) && (sequenceNumber == o.sequenceNumber);
}
bool TimestampUtil::bpv6_creation_timestamp_t::operator!=(const bpv6_creation_timestamp_t & o) const {
    return (secondsSinceStartOfYear2000 != o.secondsSinceStartOfYear2000) || (sequenceNumber != o.sequenceNumber);
}
bool TimestampUtil::bpv6_creation_timestamp_t::operator<(const bpv6_creation_timestamp_t & o) const {
    if (secondsSinceStartOfYear2000 == o.secondsSinceStartOfYear2000) {
        return (sequenceNumber < o.sequenceNumber);
    }
    return (secondsSinceStartOfYear2000 < o.secondsSinceStartOfYear2000);
}
std::ostream& operator<<(std::ostream& os, const TimestampUtil::bpv6_creation_timestamp_t & o) {
    os << "secondsSinceStartOfYear2000: " << o.secondsSinceStartOfYear2000 << ", sequenceNumber: " << o.sequenceNumber;
    return os;
}
uint64_t TimestampUtil::bpv6_creation_timestamp_t::SerializeBpv6(uint8_t * serialization) const {
    uint8_t * const serializationBase = serialization;
    serialization += SdnvEncodeU64BufSize10(serialization, secondsSinceStartOfYear2000);
    serialization += SdnvEncodeU64BufSize10(serialization, sequenceNumber);
    return serialization - serializationBase;
}
uint64_t TimestampUtil::bpv6_creation_timestamp_t::SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const {
    uint8_t * serializationBase = serialization;
    uint64_t thisSerializationSize;

    thisSerializationSize = SdnvEncodeU64(serialization, secondsSinceStartOfYear2000, bufferSize); //if zero returned on failure will only malform the bundle but not cause memory overflow
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    thisSerializationSize = SdnvEncodeU64(serialization, sequenceNumber, bufferSize);
    serialization += thisSerializationSize;
    //bufferSize -= thisSerializationSize; //not needed

    return serialization - serializationBase;
}
uint64_t TimestampUtil::bpv6_creation_timestamp_t::GetSerializationSizeBpv6() const {
    return SdnvGetNumBytesRequiredToEncode(secondsSinceStartOfYear2000) + SdnvGetNumBytesRequiredToEncode(sequenceNumber);
}
bool TimestampUtil::bpv6_creation_timestamp_t::DeserializeBpv6(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;

    secondsSinceStartOfYear2000 = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    sequenceNumber = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;
    //bufferSize -= sdnvSize; //not needed

    *numBytesTakenToDecode = static_cast<uint8_t>(serialization - serializationBase);
    return true;
}
void TimestampUtil::bpv6_creation_timestamp_t::SetZero() {
    secondsSinceStartOfYear2000 = 0;
    sequenceNumber = 0;
}
boost::posix_time::ptime TimestampUtil::bpv6_creation_timestamp_t::GetPtime() const {
    return TimestampUtil::GetRfc5050Epoch() + boost::posix_time::seconds(secondsSinceStartOfYear2000);
}
void TimestampUtil::bpv6_creation_timestamp_t::SetFromPtime(const boost::posix_time::ptime & posixTimeValue) {
    const boost::posix_time::time_duration diff = posixTimeValue - TimestampUtil::GetRfc5050Epoch();
    secondsSinceStartOfYear2000 = static_cast<uint64_t>(diff.total_seconds());
}
std::string TimestampUtil::bpv6_creation_timestamp_t::GetUtcTimestampString(bool forFileName) const {
    return TimestampUtil::GetUtcTimestampStringFromPtime(GetPtime(), forFileName);
}
void TimestampUtil::bpv6_creation_timestamp_t::SetTimeFromNow() {
    SetFromPtime(boost::posix_time::microsec_clock::universal_time());
}


//Bpv7
TimestampUtil::bpv7_creation_timestamp_t::bpv7_creation_timestamp_t() : millisecondsSinceStartOfYear2000(0), sequenceNumber(0) { } //a default constructor: X()
TimestampUtil::bpv7_creation_timestamp_t::bpv7_creation_timestamp_t(uint64_t paramMillisecondsSinceStartOfYear2000, uint32_t paramSequenceNumber) :
    millisecondsSinceStartOfYear2000(paramMillisecondsSinceStartOfYear2000), sequenceNumber(paramSequenceNumber) { }
TimestampUtil::bpv7_creation_timestamp_t::~bpv7_creation_timestamp_t() { } //a destructor: ~X()
TimestampUtil::bpv7_creation_timestamp_t::bpv7_creation_timestamp_t(const bpv7_creation_timestamp_t& o) : millisecondsSinceStartOfYear2000(o.millisecondsSinceStartOfYear2000), sequenceNumber(o.sequenceNumber) { } //a copy constructor: X(const X&)
TimestampUtil::bpv7_creation_timestamp_t::bpv7_creation_timestamp_t(bpv7_creation_timestamp_t&& o) : millisecondsSinceStartOfYear2000(o.millisecondsSinceStartOfYear2000), sequenceNumber(o.sequenceNumber) { } //a move constructor: X(X&&)
TimestampUtil::bpv7_creation_timestamp_t& TimestampUtil::bpv7_creation_timestamp_t::operator=(const bpv7_creation_timestamp_t& o) { //a copy assignment: operator=(const X&)
    millisecondsSinceStartOfYear2000 = o.millisecondsSinceStartOfYear2000;
    sequenceNumber = o.sequenceNumber;
    return *this;
}
TimestampUtil::bpv7_creation_timestamp_t& TimestampUtil::bpv7_creation_timestamp_t::operator=(bpv7_creation_timestamp_t && o) { //a move assignment: operator=(X&&)
    millisecondsSinceStartOfYear2000 = o.millisecondsSinceStartOfYear2000;
    sequenceNumber = o.sequenceNumber;
    return *this;
}
bool TimestampUtil::bpv7_creation_timestamp_t::operator==(const bpv7_creation_timestamp_t & o) const {
    return (millisecondsSinceStartOfYear2000 == o.millisecondsSinceStartOfYear2000) && (sequenceNumber == o.sequenceNumber);
}
bool TimestampUtil::bpv7_creation_timestamp_t::operator!=(const bpv7_creation_timestamp_t & o) const {
    return (millisecondsSinceStartOfYear2000 != o.millisecondsSinceStartOfYear2000) || (sequenceNumber != o.sequenceNumber);
}
bool TimestampUtil::bpv7_creation_timestamp_t::operator<(const bpv7_creation_timestamp_t & o) const {
    if (millisecondsSinceStartOfYear2000 == o.millisecondsSinceStartOfYear2000) {
        return (sequenceNumber < o.sequenceNumber);
    }
    return (millisecondsSinceStartOfYear2000 < o.millisecondsSinceStartOfYear2000);
}
std::ostream& operator<<(std::ostream& os, const TimestampUtil::bpv7_creation_timestamp_t & o) {
    os << "millisecondsSinceStartOfYear2000: " << o.millisecondsSinceStartOfYear2000 << ", sequenceNumber: " << o.sequenceNumber;
    return os;
}
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
uint64_t TimestampUtil::bpv7_creation_timestamp_t::SerializeBpv7(uint8_t * serialization) const {
    return CborTwoUint64ArraySerialize(serialization, millisecondsSinceStartOfYear2000, sequenceNumber);
}
uint64_t TimestampUtil::bpv7_creation_timestamp_t::SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) const {
    return CborTwoUint64ArraySerialize(serialization, millisecondsSinceStartOfYear2000, sequenceNumber, bufferSize);
}
uint64_t TimestampUtil::bpv7_creation_timestamp_t::GetSerializationSize() const {
    return CborTwoUint64ArraySerializationSize(millisecondsSinceStartOfYear2000, sequenceNumber);
}
bool TimestampUtil::bpv7_creation_timestamp_t::DeserializeBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize) {
    return CborTwoUint64ArrayDeserialize(serialization, numBytesTakenToDecode, bufferSize, millisecondsSinceStartOfYear2000, sequenceNumber);
}
void TimestampUtil::bpv7_creation_timestamp_t::SetZero() {
    millisecondsSinceStartOfYear2000 = 0;
    sequenceNumber = 0;
}

boost::posix_time::ptime TimestampUtil::bpv7_creation_timestamp_t::GetPtime() const {
    return TimestampUtil::GetRfc5050Epoch() + boost::posix_time::milliseconds(millisecondsSinceStartOfYear2000);
}

void TimestampUtil::bpv7_creation_timestamp_t::SetFromPtime(const boost::posix_time::ptime & posixTimeValue) {
    const boost::posix_time::time_duration diff = posixTimeValue - TimestampUtil::GetRfc5050Epoch();
    millisecondsSinceStartOfYear2000 = static_cast<uint64_t>(diff.total_milliseconds());
}

std::string TimestampUtil::bpv7_creation_timestamp_t::GetUtcTimestampString(bool forFileName) const {
    return TimestampUtil::GetUtcTimestampStringFromPtime(GetPtime(), forFileName);
}

void TimestampUtil::bpv7_creation_timestamp_t::SetTimeFromNow() {
    SetFromPtime(boost::posix_time::microsec_clock::universal_time());
}
