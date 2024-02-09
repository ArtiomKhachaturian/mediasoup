#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"

namespace RTC
{

class MkvBufferedReader;
enum class MkvReadResult;

class WebMDeserializer : public MediaFrameDeserializer
{
    class TrackInfo;
public:
    WebMDeserializer();
    ~WebMDeserializer() final;
    // impl. of RtpMediaFrameDeserializer
    MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<MemoryBuffer>& buffer) final;
    std::vector<std::shared_ptr<const MediaFrame>> ReadNextFrames(size_t trackIndex,
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