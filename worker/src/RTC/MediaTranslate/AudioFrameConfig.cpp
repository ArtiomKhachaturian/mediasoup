#define MS_CLASS "RTC::AudioFrameConfig"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "Logger.hpp"

namespace RTC
{

void AudioFrameConfig::SetChannelCount(uint8_t channelCount)
{
    MS_ASSERT(channelCount, "channels count must be greater than zero");
    _channelCount = channelCount;
}

void AudioFrameConfig::SetBitsPerSample(uint8_t bitsPerSample)
{
    MS_ASSERT(bitsPerSample, "bits per sample must be greater than zero");
    MS_ASSERT(0U == bitsPerSample % 8, "bits per sample must be a multiple of 8");
    _bitsPerSample = bitsPerSample;
}

bool AudioFrameConfig::operator == (const AudioFrameConfig& other) const
{
    return &other == this || (GetChannelCount() == other.GetChannelCount() &&
                              GetBitsPerSample() == other.GetBitsPerSample() &&
                              IsCodecSpecificDataEqual(other));
}

bool AudioFrameConfig::operator != (const AudioFrameConfig& other) const
{
    if (&other != this) {
        return GetChannelCount() != other.GetChannelCount() ||
               GetBitsPerSample() != other.GetBitsPerSample() ||
               !IsCodecSpecificDataEqual(other);
    }
    return false;
}

std::string AudioFrameConfig::ToString() const
{
    return std::to_string(GetChannelCount()) + " channels, " +
           std::to_string(GetBitsPerSample()) + " bits";
}


} // namespace RTC