#pragma once

#include "common.hpp"
#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"

namespace RTC
{

class RtpPacketsCollector;

class ConsumerTranslator
{
public:
	virtual ~ConsumerTranslator() = default;
	virtual void SetLanguage(MediaLanguage language) = 0;
	virtual MediaLanguage GetLanguage() const = 0;
	virtual void SetVoice(MediaVoice voice) = 0;
	virtual MediaVoice GetVoice() const = 0;
    virtual const std::string& GetId() const = 0;
    // return 0 if failed
    virtual uint64_t Bind(uint32_t producerAudioSsrc, const std::string& producerId,
                          const std::shared_ptr<RtpPacketsCollector>& output) = 0;
    virtual void UnBind(uint64_t bindId) = 0;
};

}
