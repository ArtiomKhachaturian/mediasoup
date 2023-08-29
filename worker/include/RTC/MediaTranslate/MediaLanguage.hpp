#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace RTC
{

enum class MediaVoice;

enum class MediaLanguage
{
    English,
    Italian,
    Spain,
    Thai,
    French,
    German,
    Russian,
    Arabic,
    Farsi
};

std::string_view MediaLanguageToString(const std::optional<MediaLanguage>& language);
std::optional<MediaLanguage> MediaLanguageFromString(const std::string_view& language);
nlohmann::json GetTargetLanguageCmd(MediaLanguage to, MediaVoice voice,
                                    const std::optional<MediaLanguage>& from = std::nullopt);

} // namespace RTC
