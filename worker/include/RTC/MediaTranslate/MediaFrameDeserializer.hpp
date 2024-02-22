#pragma once
#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include <memory>
#include <optional>

namespace RTC
{

class MediaFrame;
class Buffer;

class MediaFrameDeserializer
{
public:
    MediaFrameDeserializer(const MediaFrameDeserializer&) = delete;
    MediaFrameDeserializer(MediaFrameDeserializer&&) = delete;
    virtual ~MediaFrameDeserializer() = default;
    virtual MediaFrameDeserializeResult Add(const std::shared_ptr<Buffer>& buffer) = 0;
    virtual void Clear() {}
    // read all available frames,
    // timestamp of media frames is offset from the beginning of the stream:
    // 1st frame has zero timestamp/offset
    virtual std::shared_ptr<MediaFrame> ReadNextFrame(size_t trackIndex,
                                                      MediaFrameDeserializeResult* result = nullptr) = 0;
    // tracks info maybe actual after 1st calling of 'AddBuffer'
    virtual size_t GetTracksCount() const = 0; // all tracks, including subtitles
    virtual std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const = 0;
    virtual void SetClockRate(size_t /*trackIndex*/, uint32_t /*clockRate*/) {}
protected:
    MediaFrameDeserializer() = default;
};

} // namespace RTC
