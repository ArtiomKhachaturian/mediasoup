#pragma once
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include <absl/container/flat_hash_map.h>
#include <mkvmuxer/mkvmuxer.h>

namespace RTC
{

class MediaSink;
class MediaFrame;
class RtpCodecMimeType;
class AudioFrameConfig;
class VideoFrameConfig;

class MkvBufferedWriter : private mkvmuxer::IMkvWriter
{
   class MkvFrame;
   enum class EnqueueResult;
public:
    MkvBufferedWriter(uint32_t ssrc, MediaSink* sink, const char* app);
    ~MkvBufferedWriter() override;
    bool IsInitialized() const { return _initialized; }
    bool HasAudioTracks() const { return !_audioTracks.empty(); }
    bool HasVideoTracks() const { return !_videoTracks.empty(); }
    // AddAudioTrack/AddVideoTrack returns the track number or zero if failed
    uint64_t AddAudioTrack(int32_t sampleRate = 0, int32_t channels = 0);
    uint64_t AddVideoTrack(int32_t width = 0, int32_t height = 0);
    bool AddFrame(uint64_t trackNumber,
                  const std::shared_ptr<const MediaFrame>& mediaFrame,
                  uint64_t mkvTimestamp);
    bool SetTrackCodec(uint64_t trackNumber, const char* codec);
    bool SetTrackCodec(uint64_t trackNumber, const RtpCodecMimeType& mime);
    bool SetAudioSampleRate(uint64_t trackNumber, uint32_t sampleRate, bool opusCodec);
    void SetTrackSettings(uint64_t trackNumber, const std::shared_ptr<const AudioFrameConfig>& config);
    void SetTrackSettings(uint64_t trackNumber, const std::shared_ptr<const VideoFrameConfig>& config);
private:
    static bool SetCodecSpecific(mkvmuxer::Track* track,
                                 const std::shared_ptr<const MemoryBuffer>& specific);
    void WriteMediaPayloadToSink();
    bool HasWroteMedia() const { return _wroteMedia; }
    void ReserveBuffer() { _buffer.Reserve(1024); } // 1kb chunk reserved
    bool IsValidForTracksAdding() const;
    EnqueueResult EnqueueFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                               uint64_t mkvTimestamp, uint64_t trackNumber);
    bool WriteFrames(uint64_t mkvTimestamp);
    mkvmuxer::AudioTrack* GetAudioTrack(uint64_t trackNumber) const;
    mkvmuxer::VideoTrack* GetVideoTrack(uint64_t trackNumber) const;
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
    bool _startMediaSinkWriting = false;
    int32_t _nextNumber = 0;
    absl::flat_hash_map<uint64_t,  mkvmuxer::AudioTrack*> _audioTracks;
    absl::flat_hash_map<uint64_t,  mkvmuxer::VideoTrack*> _videoTracks;
    uint64_t _mkvVideoLastTimestamp = 0ULL;
    uint64_t _mkvAudioLastTimestamp = 0ULL;
    std::vector<MkvFrame> _mkvFrames;
    SimpleMemoryBuffer _buffer;
};

}