#pragma once

#include <cstdint>

namespace RTC
{

struct RtpMediaPacketInfo 
{
	uint32_t _ssrc = 0U;
    uint16_t _sequenceNumber = 0U;
};

} // namespace RTC