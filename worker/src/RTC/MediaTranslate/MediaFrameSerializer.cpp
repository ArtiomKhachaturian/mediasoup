#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{

MediaFrameSerializer::MediaFrameSerializer(uint32_t ssrc, const RtpCodecMimeType& mime)
    : _ssrc(ssrc)
    , _mime(mime)
{
}

std::string_view MediaFrameSerializer::GetFileExtension() const
{
    return MimeSubTypeToString(GetMimeType().GetSubtype());
}

} // namespace RTC
