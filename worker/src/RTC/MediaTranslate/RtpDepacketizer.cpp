#define MS_CLASS "RTC::RtpDepacketizer"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpDepacketizer::RtpDepacketizer(const RtpCodecMimeType& codecMimeType)
    : _codecMimeType(codecMimeType)
{
}

std::unique_ptr<RtpDepacketizer> RtpDepacketizer::create(const RtpCodecMimeType& mimeType)
{
    switch (mimeType.type) {
        case RtpCodecMimeType::Type::UNSET:
            break;
        case RtpCodecMimeType::Type::AUDIO:
            switch (mimeType.subtype) {
                case RtpCodecMimeType::Subtype::MULTIOPUS:
                case RtpCodecMimeType::Subtype::OPUS:
                    return std::make_unique<RtpDepacketizerOpus>(mimeType);
                default:
                    break;
            }
            break;
        case RtpCodecMimeType::Type::VIDEO:
            break;
    }
    return nullptr;
}

} // namespace RTC
