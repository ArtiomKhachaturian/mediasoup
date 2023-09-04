#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerVp9 : public RtpDepacketizer
{
public:
    RtpDepacketizerVp9(const RtpCodecMimeType& codecMimeType);
    // impl. of RtpDepacketizer
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
};

} // namespace RTC
