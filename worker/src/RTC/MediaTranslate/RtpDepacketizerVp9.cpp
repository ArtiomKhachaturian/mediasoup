#include "RTC/MediaTranslate/RtpDepacketizerVp9.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/RtpPacket.hpp"

namespace RTC
{

RtpDepacketizerVp9::RtpDepacketizerVp9(const RtpCodecMimeType& codecMimeType)
    : RtpDepacketizer(codecMimeType)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVp9::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload()) {
        
    }
    return nullptr;
}

} // namespace RTC
