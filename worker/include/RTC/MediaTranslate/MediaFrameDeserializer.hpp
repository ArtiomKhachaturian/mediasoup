#pragma once
#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include <memory>
#include <vector>
#include <optional>

namespace RTC
{

class MediaFrame;
class MemoryBuffer;

class MediaFrameDeserializer
{
public:
    MediaFrameDeserializer(const MediaFrameDeserializer&) = delete;
    MediaFrameDeserializer(MediaFrameDeserializer&&) = delete;
    virtual ~MediaFrameDeserializer() = default;
    virtual MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) = 0;
    virtual MediaFrameDeserializeResult ReadNextFrames(size_t trackIndex,
                                                       std::vector<std::shared_ptr<const MediaFrame>>& output) = 0;
    // tracks info maybe actual after 1st calling of 'AddBuffer'
    virtual size_t GetTracksCount() const = 0; // all tracks, including subtitles
    virtual std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const = 0;
    virtual void SetClockRate(size_t /*trackIndex*/, uint32_t /*clockRate*/) {}
    virtual void SetInitialTimestamp(uint32_t /*timestamp*/) {}
protected:
    MediaFrameDeserializer() = default;
};

} // namespace RTC
