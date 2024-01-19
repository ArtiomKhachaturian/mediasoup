#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include <memory>
#include <string>

namespace RTC
{

class RtpMediaFrame;
class OutputDevice;
class MemoryBuffer;
class AudioFrameConfig;
class VideoFrameConfig;

class RtpMediaFrameSerializer : public MediaSource
{
public:
    RtpMediaFrameSerializer(const RtpMediaFrameSerializer&) = delete;
    RtpMediaFrameSerializer(RtpMediaFrameSerializer&&) = delete;
    virtual ~RtpMediaFrameSerializer() = default;
    virtual std::string_view GetFileExtension(const RtpCodecMimeType& mimeType) const;
    // both 'RegisterAudio' & 'RegisterVideo' maybe called before and after 1st invoke of 'Push',
    // implementation should try to re-create media container if needed and send
    // restart event via 'OutputDevice::StartStream' (with restart=true)
    virtual bool AddAudio(uint32_t ssrc, uint32_t clockRate,
                          RtpCodecMimeType::Subtype codec,
                          const std::shared_ptr<const AudioFrameConfig>& config = nullptr) = 0;
    virtual bool AddVideo(uint32_t ssrc, uint32_t clockRate,
                          RtpCodecMimeType::Subtype codec,
                          const std::shared_ptr<const VideoFrameConfig>& config = nullptr) = 0;
    virtual void RemoveMedia(uint32_t ssrc) = 0;
    virtual bool Push(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) = 0;
    virtual bool IsCompatible(const RtpCodecMimeType& mimeType) const = 0;
    virtual void SetLiveMode(bool /*liveMode*/ = true) {}
protected:
    RtpMediaFrameSerializer() = default;
};

} // namespace RTC
