#pragma once

#include "common.hpp"
#include "RTC/MediaTranslate/MediaLanguage.hpp"

namespace RTC
{

class RtpPacketsCollector;
class OutputDevice;

class ProducerTranslator
{
public:
    virtual ~ProducerTranslator() = default;
    virtual std::shared_ptr<RtpPacketsCollector> AddAudio(uint32_t audioSsrc) = 0;
    virtual std::weak_ptr<RtpPacketsCollector> SetVideo(uint32_t videoSsrc, uint32_t associatedAudioSsrc) = 0;
    virtual void RemoveAudio(uint32_t audioSsrc) = 0;
    virtual void RemoveVideo(uint32_t videoSsrc) = 0;
    virtual void SetLanguage(const std::optional<MediaLanguage>& language = std::nullopt) = 0;
    virtual std::optional<MediaLanguage> GetLanguage() const = 0;
    virtual const std::string& GetId() const = 0;
    virtual void SetOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice) = 0;
};

}
