#define MS_CLASS "RTC::RtpPacketsTimeline"
#include "RTC/MediaTranslate/RtpPacketsTimeline.hpp"
#include "RTC/MediaTranslate/Timestamp.hpp"
#include "RTC/RtpDictionaries.hpp"
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

void RtpPacketsTimeline::SetTimestamp(uint32_t timestamp)
{
    const auto previous = _timestamp.exchange(timestamp);
    if (previous && timestamp >= previous) {
        _timestampDelta = timestamp - previous;
    }
}

uint32_t RtpPacketsTimeline::GetTimestamp() const
{
    return _timestamp.load();
}

void RtpPacketsTimeline::SetSeqNumber(uint16_t seqNumber)
{
    _seqNumber = seqNumber;
}

uint16_t RtpPacketsTimeline::GetSeqNumber() const
{
    return _seqNumber.load();
}

uint16_t RtpPacketsTimeline::AdvanceSeqNumber()
{
    SetSeqNumber((GetSeqNumber() + 1U) & 0xffff);
    return GetSeqNumber();
}

uint32_t RtpPacketsTimeline::GetTimestampDelta() const
{
    return _timestampDelta.load();
}

uint32_t RtpPacketsTimeline::GetEstimatedTimestampDelta(uint32_t clockRate, uint32_t ms)
{
    Timestamp offset(clockRate);
    offset.SetTime(webrtc::Timestamp::ms(ms));
    return offset.GetRtpTime();
}

} // namespace RTC
