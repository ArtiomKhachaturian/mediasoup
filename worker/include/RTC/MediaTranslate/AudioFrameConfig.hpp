#pragma once

#include "RTC/MediaTranslate/MediaFrameConfig.hpp"

namespace RTC
{

class AudioFrameConfig : public MediaFrameConfig
{
public:
    AudioFrameConfig() = default;
    void SetChannelCount(uint8_t channelCount);
    uint8_t GetChannelCount() const { return _channelCount; }
    void SetBitsPerSample(uint8_t bitsPerSample);
    uint8_t GetBitsPerSample() const { return _bitsPerSample; }
    bool operator == (const AudioFrameConfig& other) const;
    bool operator != (const AudioFrameConfig& other) const;
    // impl. of RtpMediaFrameConfig
    std::string ToString() const;
private:
    uint8_t _channelCount  = 1U;
    uint8_t _bitsPerSample = 16U;
};

} // namespace RTC
