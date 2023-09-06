#pragma once

#include <cstdint>
#include <string>

namespace RTC
{

struct RtpAudioFrameConfig
{
	uint8_t _channelCount  = 1U;
    uint8_t _bitsPerSample = 16U;
};

std::string RtpAudioFrameConfigToString(const RtpAudioFrameConfig& config);

} // namespace RTC
