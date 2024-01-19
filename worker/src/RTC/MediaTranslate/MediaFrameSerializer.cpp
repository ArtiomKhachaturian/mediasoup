#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{

std::string_view MediaFrameSerializer::GetFileExtension(const RtpCodecMimeType& mimeType) const
{
    return MimeSubTypeToString(mimeType.GetSubtype());
}

} // namespace RTC
