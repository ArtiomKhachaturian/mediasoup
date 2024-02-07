#pragma once
#include "RTC/MediaTranslate/TranslatorUnit.hpp"

namespace RTC
{

class ConsumerTranslatorSettings : public TranslatorUnit
{
public:
    virtual const std::string& GetVoiceId() const = 0;
};

} // namespace RTC
