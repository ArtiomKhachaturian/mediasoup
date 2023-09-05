#pragma once

#include <cstdint>

namespace RTC
{

struct RtpAudioFrameConfig
{
	uint8_t _channelCount  = 1U;
    uint8_t _bitsPerSample = 16U;
};

} // namespace RTC