#pragma once
#include <string>

namespace RTC
{

class ConsumerTranslator
{
public:
	virtual ~ConsumerTranslator() = default;
    virtual uint64_t GetId() const = 0;
	virtual std::string GetLanguageId() const = 0;
	virtual std::string GetVoiceId() const = 0;
};

} // namespace RTC
