#pragma once
#include "RTC/MediaTranslate/RtpMediaFrameDeserializer.hpp"

namespace mkvparser {
class Segment;
class Track;
class AudioTrack;
class VideoTrack;
class Cluster;
class BlockEntry;
class Block;
class IMkvReader;
struct EBMLHeader;
}

namespace RTC
{

class MediaFrameConfig;

class RtpWebMDeserializer : public RtpMediaFrameDeserializer
{
    class MemoryReader;
public:
    RtpWebMDeserializer();
    ~RtpWebMDeserializer() final;
    // impl. of RtpMediaFrameDeserializer
    bool AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) final;
    size_t GetTracksCount() const final;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const final;
    std::shared_ptr<AudioFrameConfig> GetTrackAudioFrameConfig(size_t trackIndex) const final;
    std::shared_ptr<VideoFrameConfig> GetTrackVideoFrameConfig(size_t trackIndex) const final;
    uint32_t GetAudioSampleRate(size_t trackIndex) const final;
    std::shared_ptr<const MediaFrame> ReadNextFrame(size_t trackIndex) final;

private:
    bool ParseLatestIncomingBuffer();
    bool ParseEBMLHeader();
    bool ParseSegment();
    const mkvparser::Track* GetTrack(size_t trackIndex) const;
    const mkvparser::AudioTrack* GetAudioTrack(size_t trackIndex) const;
    const mkvparser::VideoTrack* GetVideoTrack(size_t trackIndex) const;
    static void SetCodecSpecificData(const std::shared_ptr<MediaFrameConfig>& config,
                                     const mkvparser::Track* sourceTrack);
    
    
private:
    const std::unique_ptr<MemoryReader> _reader;
    std::unique_ptr<mkvparser::EBMLHeader> _ebmlHeader;
    mkvparser::Segment* _segment = nullptr;
    const mkvparser::Cluster* _cluster = nullptr;
    const mkvparser::BlockEntry* _blockEntry = nullptr;
    mkvparser::Block* _block = nullptr;
    bool _ok = true;
};

} // namespace RTC
