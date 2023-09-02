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
    virtual void OnConsumerEnabledChanged(const std::string& consumerId, bool enabled) = 0;
};

} // namespace RTC
