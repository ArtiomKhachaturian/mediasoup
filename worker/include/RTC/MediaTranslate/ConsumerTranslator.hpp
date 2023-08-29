#pragma once

#include "common.hpp"
#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"

namespace RTC
{

class RtpPacketsCollector;
class Producer;
class RtpStream;

class ConsumerTranslator
{
public:
	virtual ~ConsumerTranslator() = default;
	virtual void SetLanguage(MediaLanguage language) = 0;
	virtual MediaLanguage GetLanguage() const = 0;
	virtual void SetVoice(MediaVoice voice) = 0;
	virtual MediaVoice GetVoice() const = 0;
    virtual const std::string& GetId() const = 0;
    // return bind ID or zero (0) if failed
    virtual uint64_t Bind(uint32_t producerAudioSsrc, const std::string& producerId,
                          RtpPacketsCollector* output) = 0;
    uint64_t Bind(const RtpStream* producerAudioStream, const Producer* producerId,
                  RtpPacketsCollector* output);
    virtual void UnBind(uint64_t bindId) = 0;
};

}
