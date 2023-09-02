#pragma once

#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"
#include "RTC/MediaTranslate/TranslatorUnit.hpp"

namespace RTC
{

class ConsumerTranslatorSettings : public TranslatorUnit
{
public:
	virtual void SetLanguage(MediaLanguage language) = 0;
	virtual MediaLanguage GetLanguage() const = 0;
	virtual void SetVoice(MediaVoice voice) = 0;
	virtual MediaVoice GetVoice() const = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;
};

} // namespace RTC
