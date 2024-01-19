#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{

std::string_view RtpMediaFrameSerializer::GetFileExtension(const RtpCodecMimeType& mimeType) const
{
    return MimeSubTypeToString(mimeType.GetSubtype());
}

} // namespace RTC
