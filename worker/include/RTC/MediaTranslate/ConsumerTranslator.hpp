#pragma once

#include "common.hpp"
#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"

namespace RTC
{

class ProducerTranslator;

class ConsumerTranslator
{
public:
	virtual ~ConsumerTranslator() = default;
	virtual void SetLanguage(MediaLanguage language) = 0;
	virtual MediaLanguage GetLanguage() const = 0;
	virtual void SetVoice(MediaVoice voice) = 0;
	virtual MediaVoice GetVoice() const = 0;
    virtual const std::string& GetId() const = 0;
	virtual void AddProducerTranslator(const std::shared_ptr<ProducerTranslator>& producer) = 0;
};

}
