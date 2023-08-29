#ifndef MS_RTC_MEDIA_TRANSLATION_VOICE_HPP
#define MS_RTC_MEDIA_TRANSLATION_VOICE_HPP

#include <string>
#include <optional>

namespace RTC
{

enum class MediaVoice
{
    Abdul,
    JesusRodriguez,
    TestIrina,
    Serena,
    Ryan
};

std::string_view MediaVoiceToString(MediaVoice voice);
std::optional<MediaVoice> MediaVoiceFromString(const std::string_view& voice);

} // namespace RTC

#endif
