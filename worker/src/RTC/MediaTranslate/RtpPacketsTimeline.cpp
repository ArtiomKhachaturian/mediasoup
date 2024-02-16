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

uint16_t RtpPacketsTimeline::GetNextSeqNumber()
{
    _lastSeqNumber = (_lastSeqNumber + 1U) & 0xffff;
    return _lastSeqNumber;
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
