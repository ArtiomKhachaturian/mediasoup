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
    class WebMStream;
    class TrackInfo;
public:
    WebMDeserializer(std::unique_ptr<MkvReader> reader, bool loopback = false);
    ~WebMDeserializer() final;
    // impl. of RtpMediaFrameDeserializer
    MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) final;
    std::vector<std::shared_ptr<const MediaFrame>> ReadNextFrames(size_t trackIndex,
                                                                  size_t payloadOffset,
                                                                  MediaFrameDeserializeResult* result) final;
    size_t GetTracksCount() const final;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const final;
    void SetClockRate(size_t trackIndex, uint32_t clockRate) final;
    void SetInitialTimestamp(size_t trackIndex, uint32_t timestamp) final;
private:
    MediaFrameDeserializeResult ParseEBMLHeader();
    MediaFrameDeserializeResult ParseSegment();
    template<typename TMkvResult = long>
    static MediaFrameDeserializeResult FromMkvResult(TMkvResult result);
    template<typename TMkvResult = long>
    static const char* MkvResultToString(TMkvResult result);
private:
    const std::unique_ptr<MkvReader> _reader;
    const bool _loopback;
    std::unique_ptr<mkvparser::EBMLHeader> _ebmlHeader;
    mkvparser::Segment* _segment = nullptr;
    absl::flat_hash_map<size_t, std::unique_ptr<TrackInfo>> _tracks;
};

} // namespace RTC
