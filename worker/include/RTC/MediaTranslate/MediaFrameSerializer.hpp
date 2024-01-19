#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include <memory>
#include <string>

namespace RTC
{

class MediaFrame;
class OutputDevice;
class MemoryBuffer;
class AudioFrameConfig;
class VideoFrameConfig;

class MediaFrameSerializer : public MediaSource
{
public:
    MediaFrameSerializer(const MediaFrameSerializer&) = delete;
    MediaFrameSerializer(MediaFrameSerializer&&) = delete;
    virtual ~MediaFrameSerializer() = default;
    virtual std::string_view GetFileExtension(const RtpCodecMimeType& mimeType) const;
    // both 'RegisterAudio' & 'RegisterVideo' maybe called before and after 1st invoke of 'Push',
    // implementation should try to re-create media container if needed and send
    // restart event via 'MediaSink::StartStream' (with restart=true)
    virtual bool AddAudio(uint32_t ssrc, uint32_t clockRate,
                          RtpCodecMimeType::Subtype codec,
                          const std::shared_ptr<const AudioFrameConfig>& config = nullptr) = 0;
    virtual bool AddVideo(uint32_t ssrc, uint32_t clockRate,
                          RtpCodecMimeType::Subtype codec,
                          const std::shared_ptr<const VideoFrameConfig>& config = nullptr) = 0;
    virtual void RemoveMedia(uint32_t ssrc) = 0;
    virtual bool Push(uint32_t ssrc, const std::shared_ptr<const MediaFrame>& mediaFrame) = 0;
    virtual bool IsCompatible(const RtpCodecMimeType& mimeType) const = 0;
    virtual void SetLiveMode(bool /*liveMode*/ = true) {}
protected:
    MediaFrameSerializer() = default;
};

} // namespace RTC
