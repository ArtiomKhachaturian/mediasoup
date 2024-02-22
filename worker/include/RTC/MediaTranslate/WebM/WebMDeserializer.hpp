#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/Buffers/BufferAllocations.hpp"

namespace RTC
{

class MkvBufferedReader;
enum class MkvReadResult;

class WebMDeserializer : public BufferAllocations<MediaFrameDeserializer>
{
    class TrackInfo;
public:
    WebMDeserializer(const std::weak_ptr<BufferAllocator>& allocator);
    ~WebMDeserializer() final;
    // impl. of RtpMediaFrameDeserializer
    MediaFrameDeserializeResult Add(const std::shared_ptr<Buffer>& buffer) final;
    void Clear() final;
    std::shared_ptr<MediaFrame> ReadNextFrame(size_t trackIndex,
                                              MediaFrameDeserializeResult* result) final;
    size_t GetTracksCount() const final;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const final;
    void SetClockRate(size_t trackIndex, uint32_t clockRate) final;
private:
    static MediaFrameDeserializeResult FromMkvReadResult(MkvReadResult result);
private:
    const std::unique_ptr<MkvBufferedReader> _reader;
    absl::flat_hash_map<size_t, std::unique_ptr<TrackInfo>> _tracks;
};

} // namespace RTC
