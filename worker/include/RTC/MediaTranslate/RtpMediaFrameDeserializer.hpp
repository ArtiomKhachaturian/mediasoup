#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <optional>

namespace RTC
{

class MemoryBuffer;
class MediaFrame;
class AudioFrameConfig;
class VideoFrameConfig;

class RtpMediaFrameDeserializer
{
public:
    RtpMediaFrameDeserializer(const RtpMediaFrameDeserializer&) = delete;
    RtpMediaFrameDeserializer(RtpMediaFrameDeserializer&&) = delete;
    virtual ~RtpMediaFrameDeserializer() = default;
    virtual bool AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) = 0;
    // tracks info maybe actual after 1st calling of 'AddBuffer'
    virtual size_t GetTracksCount() const = 0;
    virtual std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const = 0;
    virtual std::shared_ptr<AudioFrameConfig> GetTrackAudioFrameConfig(size_t trackIndex) const = 0;
    virtual std::shared_ptr<VideoFrameConfig> GetTrackVideoFrameConfig(size_t trackIndex) const = 0;
    // return 0 for video tracks
    virtual uint32_t GetAudioSampleRate(size_t trackIndex) const = 0;
    virtual std::shared_ptr<const MediaFrame> ReadNextFrame(size_t trackIndex) = 0;

protected:
    RtpMediaFrameDeserializer() = default;
};

} // namespace RTC
