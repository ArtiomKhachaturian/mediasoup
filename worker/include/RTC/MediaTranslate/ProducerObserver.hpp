#pragma once

#include <string>

namespace RTC
{

class RtpCodecMimeType;

class ProducerObserver
{
public:
    virtual ~ProducerObserver() = default;
    virtual void onProducerStreamRegistered(const std::string& producerId,
                                            bool audio, uint32_t ssrc,
                                            uint32_t mappedSsrc, bool registered) = 0;
    virtual void OnProducerPauseChanged(const std::string& producerId, bool pause) = 0;
    virtual void OnProducerLanguageChanged(const std::string& producerId,
                                           const std::optional<MediaLanguage>& from,
                                           const std::optional<MediaLanguage>& to) = 0;
};

} // namespace RTC
