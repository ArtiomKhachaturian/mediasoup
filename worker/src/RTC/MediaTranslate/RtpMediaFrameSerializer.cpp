#define MS_CLASS "RTC::RtpMediaFrameSerializer"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpWebMSerializer.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "Logger.hpp"

namespace RTC
{

std::unique_ptr<RtpMediaFrameSerializer> RtpMediaFrameSerializer::create(const RtpCodecMimeType& mimeType)
{
    switch (mimeType.type) {
        case RtpCodecMimeType::Type::AUDIO:
            switch (mimeType.subtype) {
                case RtpCodecMimeType::Subtype::OPUS:
                    return std::make_unique<RtpWebMSerializer>();
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
