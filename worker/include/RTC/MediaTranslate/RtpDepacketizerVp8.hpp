#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerVp8 : public RtpDepacketizer
{
public:
    RtpDepacketizerVp8(const RtpCodecMimeType& codecMimeType, uint32_t sampleRate);
    // impl. of RtpDepacketizer
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
};

} // namespace RTC
