#include "RTC/MediaTranslate/RtpPacketsTimeline.hpp"
#include "RTC/Timestamp.hpp"
#include "RTC/RtpPacket.hpp"

namespace RTC
{

void RtpPacketsTimeline::CopyPacketInfoFrom(const RtpPacket* packet)
{
    if (packet) {
        _lastSeqNumber = packet->GetSequenceNumber();
        SetLastTimestamp(packet->GetTimestamp());
    }
}

uint32_t RtpPacketsTimeline::GetNextTimestamp() const
{
    return GetLastTimestamp() + GetTimestampDelta();
}

void RtpPacketsTimeline::SetLastTimestamp(uint32_t lastTimestamp)
{
    if (_lastTimestamp != lastTimestamp) {
        if (_lastTimestamp && lastTimestamp > _lastTimestamp) {
            _timestampDelta = lastTimestamp - _lastTimestamp;
        }
        _lastTimestamp = lastTimestamp;
    }
}

void RtpPacketsTimeline::Reset()
{
    _lastTimestamp = 0U;
    _lastSeqNumber = _timestampDelta = 0U;
}

} // namespace RTC
