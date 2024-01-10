#pragma once

#include "RTC/MediaTranslate/TranslatorUnit.hpp"

namespace RTC
{

class ConsumerTranslatorSettings : public TranslatorUnit
{
public:
	virtual std::optional<FBS::TranslationPack::Voice> GetVoice() const = 0;
};

} // namespace RTC
