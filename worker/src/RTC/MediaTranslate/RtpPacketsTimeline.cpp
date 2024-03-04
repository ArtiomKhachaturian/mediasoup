#define MS_CLASS "RTC::RtpPacketsTimeline"
#include "RTC/MediaTranslate/RtpPacketsTimeline.hpp"
#include "RTC/Timestamp.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/Timestamp.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsTimeline::RtpPacketsTimeline(uint32_t clockRate, const RtpCodecMimeType& mime)
    : _clockRate(clockRate)
{
    switch (mime.GetSubtype()) {
        case RtpCodecMimeType::Subtype::OPUS:
        case RtpCodecMimeType::Subtype::MULTIOPUS:
            // 20 ms is default interval in OPUS encoder
            _timestampDelta = GetEstimatedTimestampDelta(_clockRate, 20U);
            break;
        default:
            break;
    }
}

RtpPacketsTimeline::RtpPacketsTimeline(const RtpPacketsTimeline& other)
    : _clockRate(other.GetClockRate())
    , _timestamp(other.GetTimestamp())
    , _seqNumber(other.GetSeqNumber())
    , _timestampDelta(other.GetTimestampDelta())
{
}

uint32_t RtpPacketsTimeline::AdvanceTimestamp(uint32_t offset)
{
    if (offset) {
        SetTimestamp(GetTimestamp() + offset);
    }
    return GetTimestamp();
}

uint32_t RtpPacketsTimeline::AdvanceTimestamp(const Timestamp& offset)
{
    MS_ASSERT(GetClockRate() == offset.GetClockRate(), "clock rate mistmatch");
    return AdvanceTimestamp(offset.GetRtpTime());
}

void RtpPacketsTimeline::SetTimestamp(uint32_t timestamp)
{
    const auto previous = _timestamp.exchange(timestamp);
    if (previous) {
        MS_ASSERT(timestamp >= previous, "incorrect timestamps order");
        _timestampDelta = timestamp - previous;
    }
}

void RtpPacketsTimeline::SetSeqNumber(uint16_t seqNumber)
{
    const auto previous = _seqNumber.exchange(seqNumber);
    if (previous) {
        MS_ASSERT(seqNumber >= previous, "incorrect sequence numbers order");
    }
}

uint16_t RtpPacketsTimeline::AdvanceSeqNumber()
{
    SetSeqNumber((GetSeqNumber() + 1U) & 0xffff);
    return GetSeqNumber();
}

uint32_t RtpPacketsTimeline::GetEstimatedTimestampDelta(uint32_t clockRate, uint32_t ms)
{
    Timestamp offset(clockRate);
    offset.SetTime(webrtc::Timestamp::ms(ms));
    return offset.GetRtpTime();
}

} // namespace RTC
