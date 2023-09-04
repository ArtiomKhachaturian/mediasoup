#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerOpus : public RtpDepacketizer
{
public:
    RtpDepacketizerOpus(const RtpCodecMimeType& codecMimeType);
    // impl. of RtpDepacketizer
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
private:
    std::allocator<uint8_t> _payloadAllocator;
};

} // namespace RTC
