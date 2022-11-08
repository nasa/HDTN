#include <string.h>
#include "BPing.h"
#include "Logger.h"
#include <boost/format.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

struct bping_data_t {
    uint64_t sequence;
    boost::posix_time::ptime sendTime;
};

BPing::BPing() :
    BpSourcePattern(),
    m_bpingSequenceNumber(0)
{

}

BPing::~BPing() {}


uint64_t BPing::GetNextPayloadLength_Step1() {
    return sizeof(bping_data_t);
}
bool BPing::CopyPayload_Step2(uint8_t * destinationBuffer) {
    bping_data_t bPingData;
    bPingData.sequence = m_bpingSequenceNumber++;
    bPingData.sendTime = boost::posix_time::microsec_clock::universal_time();
    memcpy(destinationBuffer, &bPingData, sizeof(bPingData));
    return true;
}

bool BPing::ProcessNonAdminRecordBundlePayload(const uint8_t * data, const uint64_t size) {
    const boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    bping_data_t bPingData;
    if (size != sizeof(bPingData)) {
        LOG_ERROR(subprocess) << "error in BPing::ProcessNonAdminRecordBundlePayload: received payload size not " << sizeof(bPingData);
        return false;
    }
    memcpy(&bPingData, data, sizeof(bPingData));
    const boost::posix_time::time_duration diff = nowTime - bPingData.sendTime;
    const double millisecs = (static_cast<double>(diff.total_microseconds())) * 0.001;
    static const boost::format fmtTemplate("Ping received: sequence=%d, took %0.3f milliseconds");
    boost::format fmt(fmtTemplate);
    fmt % bPingData.sequence % millisecs;
    LOG_INFO(subprocess) << fmt.str();
    return true;
}
