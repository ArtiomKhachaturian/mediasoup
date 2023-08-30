#pragma once

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

inline constexpr MediaVoice DefaultMediaVoice() { return MediaVoice::Abdul; }

std::string_view MediaVoiceToString(MediaVoice voice);

std::optional<MediaVoice> MediaVoiceFromString(const std::string_view& voice);

} // namespace RTC
