#pragma once

#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "RTC/MediaTranslate/TranslatorUnit.hpp"

namespace RTC
{

class ProducerTranslatorSettings : public TranslatorUnit
{
public:
	virtual void SetLanguage(const std::optional<MediaLanguage>& language = std::nullopt) = 0;
    virtual std::optional<MediaLanguage> GetLanguage() const = 0;
};

} // namespace RTC