#pragma once

#include <string>

namespace RTC
{

class RtpPacketsCollector;

class ConsumerObserver
{
public:
    virtual ~ConsumerObserver() = default;
    virtual void OnConsumerPauseChanged(const std::string& consumerId, bool pause) = 0;
    virtual void OnConsumerLanguageChanged(const std::string& consumerId) = 0;
    virtual void OnConsumerVoiceChanged(const std::string& consumerId) = 0;
    virtual uint64_t BindToProducer(const std::string& consumerId,
                                    uint32_t producerAudioSsrc, const std::string& producerId,
                                    RtpPacketsCollector* output) = 0;
    virtual void UnBindFromProducer(uint64_t bindId) = 0;
};

} // namespace RTC
