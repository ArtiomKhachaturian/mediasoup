#define MS_CLASS "RTC::RtpDepacketizer"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpDepacketizer::RtpDepacketizer(const RtpCodecMimeType& mimeType, uint32_t clockRate)
    : _mimeType(mimeType)
    , _clockRate(clockRate)
{
    MS_ASSERT(_mimeType.IsMediaCodec(), "invalid media codec");
}

std::unique_ptr<RtpDepacketizer> RtpDepacketizer::Create(const RtpCodecMimeType& mimeType,
                                                         uint32_t clockRate)
{
    switch (mimeType.GetType()) {
        case RtpCodecMimeType::Type::AUDIO:
            switch (mimeType.GetSubtype()) {
                case RtpCodecMimeType::Subtype::MULTIOPUS:
                case RtpCodecMimeType::Subtype::OPUS:
                    return std::make_unique<RtpDepacketizerOpus>(mimeType, clockRate);
                default:
                    break;
            }
            break;
        case RtpCodecMimeType::Type::VIDEO:
            switch (mimeType.GetSubtype()) {
                case RtpCodecMimeType::Subtype::VP8:
                case RtpCodecMimeType::Subtype::VP9:
                    return std::make_unique<RtpDepacketizerVpx>(mimeType, clockRate);
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return nullptr;
}

} // namespace RTC
