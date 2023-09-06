#pragma once

#include <cstdint>
#include <string>

namespace RTC
{

struct RtpVideoFrameConfig
{
	int32_t _width    = 0;
    int32_t _height   = 0;
    double _frameRate = 0.; // optional
    bool HasResolution() const { return _width > 0 && _height > 0; }
};

std::string RtpVideoFrameConfigToString(const RtpVideoFrameConfig& config);

} // namespace RTC
