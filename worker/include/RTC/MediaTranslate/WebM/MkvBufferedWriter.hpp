#pragma once
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include <absl/container/flat_hash_map.h>
#include <mkvmuxer/mkvmuxer.h>

namespace RTC
{

class MediaSink;
class MediaFrame;
class AudioFrameConfig;
class VideoFrameConfig;

class MkvBufferedWriter : private mkvmuxer::IMkvWriter
{
   class MkvFrame;
   enum class EnqueueResult;
public:
    MkvBufferedWriter(uint32_t ssrc, MediaSink* sink, const char* app);
    ~MkvBufferedWriter() final;
    bool IsInitialized() const { return _initialized; }
    bool HasAudioTracks() const { return _audioTracksCount > 0UL; }
    bool HasVideoTracks() const { return _videoTracksCount > 0UL; }
    bool AddFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                  uint64_t mkvTimestamp, int32_t trackNumber);
    bool AddAudioTrack(int32_t number, int32_t sampleRate = 0, int32_t channels = 0);
    bool AddVideoTrack(int32_t number, int32_t width = 0, int32_t height = 0);
    bool SetTrackCodec(int32_t number, const char* codec);
    bool SetAudioSampleRate(int32_t number, uint32_t sampleRate, bool opusCodec);
    void SetTrackSettings(int32_t number, const std::shared_ptr<const AudioFrameConfig>& config);
    void SetTrackSettings(int32_t number, const std::shared_ptr<const VideoFrameConfig>& config);
private:
    static bool SetCodecSpecific(mkvmuxer::Track* track,
                                 const std::shared_ptr<const MemoryBuffer>& specific);
    void WriteMediaPayloadToSink();
    bool HasWroteMedia() const { return _wroteMedia; }
    void ReserveBuffer() { _buffer.Reserve(1024); } // 1kb chunk reserved
    mkvmuxer::Track* GetTrack(int32_t number) const;
    bool IsValidForTracksAdding() const;
    EnqueueResult EnqueueFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                               uint64_t mkvTimestamp, int32_t trackNumber);
    bool WriteFrames(uint64_t mkvTimestamp);
    // impl. of mkvmuxer::IMkvWriter
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final;
    mkvmuxer::int64 Position() const final;
    mkvmuxer::int32 Position(mkvmuxer::int64 /*position*/) final { return -1; }
    bool Seekable() const final { return false; }
    void ElementStartNotify(mkvmuxer::uint64 /*elementId*/, mkvmuxer::int64 /*position*/) final {}
private:
    const uint32_t _ssrc;
    MediaSink* const _sink;
    mkvmuxer::Segment _segment;
    const bool _initialized;
    bool _wroteMedia = false;
    absl::flat_hash_map<int32_t, uint64_t> _tracksReference;
    size_t _audioTracksCount = 0UL;
    size_t _videoTracksCount = 0UL;
    uint64_t _mkvVideoLastTimestamp = 0ULL;
    uint64_t _mkvAudioLastTimestamp = 0ULL;
    std::vector<MkvFrame> _mkvFrames;
    SimpleMemoryBuffer _buffer;
};

}
