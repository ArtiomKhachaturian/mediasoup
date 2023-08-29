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

struct TranslationPack
{
    MediaLanguage _languageTo;
    MediaVoice _voice;
    std::optional<MediaLanguage> _languageFrom;
    TranslationPack();
    TranslationPack(MediaLanguage languageTo, MediaVoice voice,
                    const std::optional<MediaLanguage>& languageFrom = std::nullopt);
};

// TODO: RU for tests only, remove it for production, should be auto (std::nullopt)
inline constexpr std::optional<MediaLanguage> DefaultInputMediaLanguage() { return MediaLanguage::Russian; }

// TODO: EN for tests only, remove it for production, should be Spain
inline constexpr MediaLanguage DefaultOutputMediaLanguage() { return MediaLanguage::English; }

std::string_view MediaLanguageToString(const std::optional<MediaLanguage>& language);

std::optional<MediaLanguage> MediaLanguageFromString(const std::string_view& language);

nlohmann::json GetTargetLanguageCmd(MediaLanguage languageTo, MediaVoice voice,
                                    const std::optional<MediaLanguage>& languageFrom = std::nullopt);

nlohmann::json GetTargetLanguageCmd(const TranslationPack& pack);

} // namespace RTC
