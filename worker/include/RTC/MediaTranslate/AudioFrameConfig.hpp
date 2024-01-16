#pragma once

#include "RTC/MediaTranslate/MediaFrameConfig.hpp"

namespace RTC
{

class MemoryBuffer;

class AudioFrameConfig : public MediaFrameConfig
{
public:
    AudioFrameConfig() = default;
    void SetChannelCount(uint8_t channelCount);
    uint8_t GetChannelCount() const { return _channelCount.load(std::memory_order_relaxed); }
    void SetBitsPerSample(uint8_t bitsPerSample);
    uint8_t GetBitsPerSample() const { return _bitsPerSample.load(std::memory_order_relaxed); }
    // impl. of RtpMediaFrameConfig
    std::string ToString() const;
private:
    std::atomic<uint8_t> _channelCount  = 1U;
    std::atomic<uint8_t> _bitsPerSample = 16U;
};

} // namespace RTC
