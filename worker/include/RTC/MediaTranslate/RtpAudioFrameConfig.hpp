#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace RTC
{

class MemoryBuffer;

struct RtpAudioFrameConfig
{
	uint8_t _channelCount  = 1U;
    uint8_t _bitsPerSample = 16U;
    std::unique_ptr<const MemoryBuffer> _codecSpecificData;
};

std::string RtpAudioFrameConfigToString(const RtpAudioFrameConfig& config);

} // namespace RTC
