#pragma once
#include <cstdint>

namespace RTC
{

class RtpPacket;
class Timestamp;

class RtpPacketsTimeline
{
public:
    RtpPacketsTimeline() = default;
    RtpPacketsTimeline(const RtpPacketsTimeline&) = default;
    void CopyPacketInfoFrom(const RtpPacket* packet);
    uint16_t GetTimestampDelta() const { return _timestampDelta; }
    uint32_t GetLastTimestamp() const { return _lastTimestamp; }
    void SetLastTimestamp(uint32_t lastTimestamp);
    uint32_t GetNextTimestamp() const;
    uint16_t GetLastSeqNumber() const { return _lastSeqNumber; }
    void SetLastSeqNumber(uint16_t seqNumber) { _lastSeqNumber = seqNumber; }
    uint16_t GetNextSeqNumber() const { return GetLastSeqNumber() + 1U;}
    void Reset();
    RtpPacketsTimeline& operator = (const RtpPacketsTimeline&) = default;
private:
    uint32_t _lastTimestamp = 0U;
    uint16_t _lastSeqNumber = 0U;
    uint16_t _timestampDelta = 0U;
};

} // namespace RTC
