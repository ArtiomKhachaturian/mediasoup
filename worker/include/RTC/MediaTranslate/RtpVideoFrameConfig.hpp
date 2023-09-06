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
};

std::string RtpVideoFrameConfigToString(const RtpVideoFrameConfig& config);

} // namespace RTC
