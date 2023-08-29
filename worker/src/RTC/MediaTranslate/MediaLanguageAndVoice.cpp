#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"

namespace {

const RTC::MediaLanguage g_allLanguages[] = {
    RTC::MediaLanguage::English,
    RTC::MediaLanguage::Italian,
    RTC::MediaLanguage::Spain,
    RTC::MediaLanguage::Thai,
    RTC::MediaLanguage::French,
    RTC::MediaLanguage::German,
    RTC::MediaLanguage::Russian,
    RTC::MediaLanguage::Arabic,
    RTC::MediaLanguage::Farsi
};

const RTC::MediaVoice g_allVoices[] = {
    RTC::MediaVoice::Abdul,
    RTC::MediaVoice::JesusRodriguez,
    RTC::MediaVoice::TestIrina,
    RTC::MediaVoice::Serena,
    RTC::MediaVoice::Ryan
};

}

namespace RTC
{

TranslationPack::TranslationPack()
    : TranslationPack(DefaultOutputMediaLanguage(), DefaultMediaVoice(), DefaultInputMediaLanguage())
{
}

TranslationPack::TranslationPack(MediaLanguage languageTo, MediaVoice voice,
                                 const std::optional<MediaLanguage>& languageFrom)
    : _languageTo(languageTo)
    , _voice(voice)
    , _languageFrom(languageFrom)
{
}

std::string_view MediaLanguageToString(const std::optional<MediaLanguage>& language)
{
    if (language.has_value()) {
        switch (language.value()) {
            case MediaLanguage::English:
                return "en";
            case MediaLanguage::Italian:
                return "it";
            case MediaLanguage::Spain:
                return "es";
            case MediaLanguage::Thai:
                return "th";
            case MediaLanguage::French:
                return "fr";
            case MediaLanguage::German:
                return "de";
            case MediaLanguage::Russian:
                return "ru";
            case MediaLanguage::Arabic:
                return "ar";
            case MediaLanguage::Farsi:
                return "fa";
            default:
                // assert
                break;
        }
        return "";
    }
    return "auto";
}

std::optional<MediaLanguage> MediaLanguageFromString(const std::string_view& str)
{
    if (!str.empty()) {
        for (const auto language : g_allLanguages) {
            if (str.compare(MediaLanguageToString(language))) {
                return language;
            }
        }
    }
    return std::nullopt;
}

std::string_view MediaVoiceToString(MediaVoice voice)
{
    switch (voice) {
        case MediaVoice::Abdul:
            return "YkxA6GRXs4A6i5cwfm1E";
        case MediaVoice::JesusRodriguez:
            return "ovxyZ1ldY23QpYBvkKx5";
        case MediaVoice::TestIrina:
            return "ovxyZ1ldY23QpYBvkKx5";
        case MediaVoice::Serena:
            return "pMsXgVXv3BLzUgSXRplE";
            break;
        case MediaVoice::Ryan:
            return "wViXBPUzp2ZZixB1xQuM";
        default:
            // assert
            break;
    }
    return "";
}

std::optional<MediaVoice> MediaVoiceFromString(const std::string_view& str)
{
    if (!str.empty()) {
        for (const auto voice : g_allVoices) {
            if (str.compare(MediaVoiceToString(voice))) {
                return voice;
            }
        }
    }
    return std::nullopt;
}

nlohmann::json GetTargetLanguageCmd(MediaLanguage languageTo, MediaVoice voice,
                                    const std::optional<MediaLanguage>& languageFrom)
{
    nlohmann::json cmd = {{
        "from",    MediaLanguageToString(languageFrom),
        "to",      MediaLanguageToString(languageTo),
        "voiceID", MediaVoiceToString(voice)
    }};
    nlohmann::json data = {
        "type", "set_target_language",
        "cmd",  cmd
    };
    return data;
}

nlohmann::json GetTargetLanguageCmd(const TranslationPack& pack)
{
    return GetTargetLanguageCmd(pack._languageTo, pack._voice, pack._languageFrom);
}

} // namespace RTC
