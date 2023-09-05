#define MS_CLASS "RTC::RtpDepacketizer"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpDepacketizerVp8.hpp"
#include "RTC/MediaTranslate/RtpDepacketizerVp9.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpDepacketizer::RtpDepacketizer(const RtpCodecMimeType& codecMimeType, uint32_t sampleRate)
    : _codecMimeType(codecMimeType)
    , _sampleRate(sampleRate)
{
    MS_ASSERT(_codecMimeType.IsMediaCodec(), "invalid media codec");
}

std::unique_ptr<RtpDepacketizer> RtpDepacketizer::create(const RtpCodecMimeType& mimeType,
                                                         uint32_t sampleRate)
{
    switch (mimeType.GetType()) {
        case RtpCodecMimeType::Type::AUDIO:
            switch (mimeType.GetSubtype()) {
                case RtpCodecMimeType::Subtype::MULTIOPUS:
                case RtpCodecMimeType::Subtype::OPUS:
                    return std::make_unique<RtpDepacketizerOpus>(mimeType, sampleRate);
                default:
                    break;
            }
            break;
        case RtpCodecMimeType::Type::VIDEO:
            switch (mimeType.GetSubtype()) {
                case RtpCodecMimeType::Subtype::VP8:
                    //return std::make_unique<RtpDepacketizerVp8>(mimeType, sampleRate);
                case RtpCodecMimeType::Subtype::VP9:
                    //return std::make_unique<RtpDepacketizerVp9>(mimeType, sampleRate);
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
