#define MS_CLASS "RTC::MediaFrameSerializer"
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "Logger.hpp"

namespace RTC
{

MediaFrameSerializer::MediaFrameSerializer(uint32_t ssrc, uint32_t clockRate,
                                           const RtpCodecMimeType& mime)
    : _ssrc(ssrc)
    , _clockRate(clockRate)
    , _mime(mime)
{
    MS_ASSERT(_clockRate, "clock rate must be greater than zero");
}

std::string_view MediaFrameSerializer::GetFileExtension() const
{
    return MimeSubTypeToString(GetMimeType().GetSubtype());
}

} // namespace RTC
