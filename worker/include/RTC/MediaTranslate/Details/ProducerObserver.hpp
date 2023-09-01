#pragma once

#include <string>

namespace RTC
{

class ProducerObserver
{
public:
    virtual ~ProducerObserver() = default;
    virtual void OnProducerPauseChanged(const std::string& producerId, bool pause) = 0;
    virtual void OnProducerLanguageChanged(const std::string& producerId) = 0;
    virtual void OnProducerAudioRemoved(const std::string& producerId, uint32_t audioSsrc) = 0;
};

} // namespace RTC