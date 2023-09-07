#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace RTC
{

class MemoryBuffer;

struct RtpVideoFrameConfig
{
	int32_t _width    = 0;
    int32_t _height   = 0;
    double _frameRate = 0.; // optional
    std::unique_ptr<const MemoryBuffer> _codecSpecificData;
    bool HasResolution() const { return _width > 0 && _height > 0; }
};

std::string RtpVideoFrameConfigToString(const RtpVideoFrameConfig& config);

} // namespace RTC
