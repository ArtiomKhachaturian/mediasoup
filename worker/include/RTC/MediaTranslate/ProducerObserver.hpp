#pragma once

#include <string>
#include <optional>

namespace RTC
{

class RtpCodecMimeType;
enum class MediaLanguage;

class ProducerObserver
{
public:
    virtual ~ProducerObserver() = default;
    virtual void onStreamAdded(const std::string& /*producerId*/,
                               uint32_t /*mappedSsrc*/,
                               const RtpCodecMimeType& /*mime*/,
                               uint32_t /*clockRate*/) {}
    virtual void onStreamRemoved(const std::string& /*producerId*/,
                                 uint32_t /*mappedSsrc*/,
                                 const RtpCodecMimeType& /*mime*/) {}
    virtual void OnPauseChanged(const std::string& /*producerId*/, bool /*pause*/) {}
    virtual void OnLanguageChanged(const std::string& /*producerId*/,
                                   const std::optional<MediaLanguage>& /*from*/,
                                   const std::optional<MediaLanguage>& /*to*/) {}
};

} // namespace RTC
