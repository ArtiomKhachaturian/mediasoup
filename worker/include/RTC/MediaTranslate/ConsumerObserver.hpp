#pragma once

#include <string>

namespace RTC
{

class RtpPacketsCollector;

class ConsumerObserver
{
public:
    virtual ~ConsumerObserver() = default;
    virtual void OnConsumerPauseChanged(const std::string& /*consumerId*/, bool /*pause*/) {}
    virtual void OnConsumerMediaStreamStarted(const std::string& /*consumerId*/, bool /*started*/) {}
};

} // namespace RTC
