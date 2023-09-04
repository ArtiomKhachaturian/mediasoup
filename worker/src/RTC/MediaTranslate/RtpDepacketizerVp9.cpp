#include "RTC/MediaTranslate/RtpDepacketizerVp9.hpp"

namespace RTC
{

RtpDepacketizerVp9::RtpDepacketizerVp9(const RtpCodecMimeType& codecMimeType)
    : RtpDepacketizer(codecMimeType)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVp9::AddPacket(const RtpPacket* packet)
{
    if (packet) {
        
    }
    return nullptr;
}

} // namespace RTC
