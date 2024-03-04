#pragma once
#include <atomic>
#include <cstdint>

namespace RTC
{

class RtpPacket;
class Timestamp;
class RtpCodecMimeType;

class RtpPacketsTimeline
{
public:
    RtpPacketsTimeline() = delete;
    RtpPacketsTimeline(uint32_t clockRate, const RtpCodecMimeType& mime);
    RtpPacketsTimeline(const RtpPacketsTimeline& other);
    RtpPacketsTimeline(RtpPacketsTimeline&&) = delete;
    uint32_t GetClockRate() const { return _clockRate; }
    void SetTimestamp(uint32_t timestamp);
    uint32_t GetTimestamp() const;
    uint32_t GetNextTimestamp() const { return GetTimestamp() + GetTimestampDelta(); }
    void SetSeqNumber(uint16_t seqNumber);
    uint16_t GetSeqNumber() const;
    // increase & return next seq. number
    uint16_t AdvanceSeqNumber();
    uint32_t GetTimestampDelta() const;
    RtpPacketsTimeline& operator = (const RtpPacketsTimeline&) = delete;
    RtpPacketsTimeline& operator = (RtpPacketsTimeline&&) = delete;
private:
    static uint32_t GetEstimatedTimestampDelta(uint32_t clockRate, uint32_t ms);
private:
    const uint32_t _clockRate;
    std::atomic<uint32_t> _timestamp = 0U;
    std::atomic<uint16_t> _seqNumber = 0U;
    std::atomic<uint32_t> _timestampDelta = 0U;
};

} // namespace RTC
