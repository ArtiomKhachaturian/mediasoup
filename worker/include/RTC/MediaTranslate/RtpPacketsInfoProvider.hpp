#pragma once
#include <cstdint>

namespace RTC
{   

class RtpPacketsInfoProvider
{
public:
     // SSRC maybe mapped or original, return zero if failed
    virtual uint8_t GetPayloadType(uint32_t ssrc) const = 0;
    virtual uint32_t GetClockRate(uint32_t ssrc) const = 0;
protected:
    virtual ~RtpPacketsInfoProvider() = default;
};

} // namespace RTC
