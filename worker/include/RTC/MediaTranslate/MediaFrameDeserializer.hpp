#pragma once
#include "RTC/RtpDictionaries.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace RTC
{

class Buffer;
class MediaFrame;
class MediaFrameDeserializedTrack;
class RtpPacketizer;
class RtpTranslatedPacket;

class MediaFrameDeserializer : public BufferAllocations<void>
{
    using TrackInfo = std::pair<std::unique_ptr<RtpPacketizer>, std::unique_ptr<MediaFrameDeserializedTrack>>;
public:
    virtual ~MediaFrameDeserializer();
    MediaFrameDeserializeResult Add(const std::shared_ptr<Buffer>& buffer);
    virtual void Clear();
    // count of all tracks, including subtitles
    size_t GetTracksCount() const { return _tracks.size(); }
    std::optional<RtpTranslatedPacket> NextPacket(size_t trackIndex);
    // tracks info maybe actual after 1st calling of 'Add'
    std::optional<RtpCodecMimeType> GetTrackType(size_t trackIndex) const;
    MediaFrameDeserializeResult GetTrackLastResult(size_t trackIndex) const;
    void SetClockRate(size_t trackIndex, uint32_t clockRate);
protected:
    MediaFrameDeserializer(const std::weak_ptr<BufferAllocator>& allocator);
    void AddTrack(const RtpCodecMimeType& type, std::unique_ptr<MediaFrameDeserializedTrack> track);
    virtual MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<Buffer>& buffer) = 0;
    virtual void ParseTracksInfo() = 0;
private:
    MediaFrameDeserializer(const MediaFrameDeserializer&) = delete;
    MediaFrameDeserializer(MediaFrameDeserializer&&) = delete;
private:
    std::vector<TrackInfo> _tracks;
};

} // namespace RTC
