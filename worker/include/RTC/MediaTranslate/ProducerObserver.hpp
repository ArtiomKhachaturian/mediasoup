#pragma once

#include <string>

namespace RTC
{

class RtpCodecMimeType;
class RtpMediaFrame;

class ProducerObserver
{
public:
    virtual ~ProducerObserver() = default;
    virtual void onStreamAdded(const std::string& producerId, uint32_t mappedSsrc,
                               const RtpCodecMimeType& mime, uint32_t clockRate) = 0;
    virtual void onStreamRemoved(const std::string& producerId, uint32_t mappedSsrc,
                                 const RtpCodecMimeType& mime) = 0;
    virtual void OnPauseChanged(const std::string& /*producerId*/, bool /*pause*/) {}
    virtual void OnLanguageChanged(const std::string& producerId,
                                   const std::optional<MediaLanguage>& from,
                                   const std::optional<MediaLanguage>& to) = 0;
    virtual void OnMediaFrameProduced(const std::string& producerId, uint32_t mappedSsrc,
                                      const std::shared_ptr<RtpMediaFrame>& mediaFrame) = 0;
};

} // namespace RTC
