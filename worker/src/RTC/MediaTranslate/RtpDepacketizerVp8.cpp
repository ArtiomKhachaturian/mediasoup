#include "RTC/MediaTranslate/RtpDepacketizerVp8.hpp"

namespace RTC
{

RtpDepacketizerVp8::RtpDepacketizerVp8(const RtpCodecMimeType& codecMimeType)
    : RtpDepacketizer(codecMimeType)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVp8::AddPacket(const RtpPacket* packet)
{
    if (packet) {
        
    }
    return nullptr;
}

} // namespace RTC
