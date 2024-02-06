#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"

namespace mkvparser {
class EBMLHeader;
class Segment;
class Cluster;
class Block;
class BlockEntry;
}

namespace RTC
{

class MkvReader;

class WebMDeserializer : public MediaFrameDeserializer
{
    class TrackInfo;
    enum class MkvResult;
public:
    WebMDeserializer(std::unique_ptr<MkvReader> reader);
    ~WebMDeserializer() final;
    // impl. of RtpMediaFrameDeserializer
    MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<MemoryBuffer>& buffer) final;
    std::vector<std::shared_ptr<const MediaFrame>> ReadNextFrames(size_t trackIndex,
                                                                  MediaFrameDeserializeResult* result) final;
    size_t GetTracksCount() const final;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const final;
    void SetClockRate(size_t trackIndex, uint32_t clockRate) final;
private:
    MediaFrameDeserializeResult ParseEBMLHeader();
    MediaFrameDeserializeResult ParseSegment();
    static MkvResult ToMkvResult(long ret);
    static MediaFrameDeserializeResult FromMkvResult(MkvResult result);
    static const char* MkvResultToString(MkvResult result);
    static const char* MkvResultToString(long result);
    static bool IsOk(MkvResult result);
    static bool MaybeOk(MkvResult result);
private:
    const std::unique_ptr<MkvReader> _reader;
    std::unique_ptr<mkvparser::EBMLHeader> _ebmlHeader;
    mkvparser::Segment* _segment = nullptr;
    absl::flat_hash_map<size_t, std::unique_ptr<TrackInfo>> _tracks;
};

} // namespace RTC
