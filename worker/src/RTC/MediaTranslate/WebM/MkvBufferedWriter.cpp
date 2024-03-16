#define MS_CLASS "RTC::MkvBufferedWriter"
#include "RTC/MediaTranslate/WebM/MkvBufferedWriter.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/Buffers/BufferAllocator.hpp"
#include "Logger.hpp"
#include <algorithm> // for stable_sort
#include <inttypes.h>

namespace {

inline bool CompareFrames(const mkvmuxer::Frame& lhs, const mkvmuxer::Frame& rhs) {
    return lhs.timestamp() < rhs.timestamp();
}

}

namespace RTC
{

enum class MkvBufferedWriter::EnqueueResult {
    Added,
    Dropped,
    Failure
};

class MkvBufferedWriter::MkvFrameMemory : public mkvmuxer::FrameMemory
{
public:
    MkvFrameMemory(std::shared_ptr<const Buffer> data);
    // impl. of mkvmuxer::FrameMemory
    const uint8_t* GetData() const final { return _data->GetData(); }
    uint64_t GetSize() const final { return _data->GetSize(); }
private:
    const std::shared_ptr<const Buffer> _data;
};

class MkvBufferedWriter::MkvBufferView : public Buffer
{
public:
    MkvBufferView(std::shared_ptr<Buffer> buffer, size_t size);
    // impl. of Buffer
    size_t GetSize() const final { return _size; }
    uint8_t* GetData() final { return _buffer->GetData(); }
    const uint8_t* GetData() const final { return _buffer->GetData(); }
private:
    const std::shared_ptr<Buffer> _buffer;
    const size_t _size;
};

MkvBufferedWriter::MkvBufferedWriter(uint64_t senderId, MediaSink* sink, const char* app,
                                     const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<mkvmuxer::IMkvWriter>(allocator)
    , _senderId(senderId)
    , _sink(sink)
    , _initialized(_segment.Init(this))
{
    if (IsInitialized()) {
        _segment.set_mode(mkvmuxer::Segment::kLive);
        _segment.set_estimate_file_duration(false);
        _segment.OutputCues(false);
        if (const auto segmentInfo = _segment.GetSegmentInfo()) {
            segmentInfo->set_writing_app(app);
            segmentInfo->set_muxing_app(app);
        }
    }
}

MkvBufferedWriter::~MkvBufferedWriter()
{
    if (IsInitialized()) {
        _segment.Finalize();
        WriteMediaPayloadToSink();
        if (_startMediaSinkWriting) {
            _sink->EndMediaWriting(_senderId);
        }
    }
}

uint64_t MkvBufferedWriter::AddAudioTrack(int32_t channels)
{
    uint64_t trackNumber = 0ULL;
    if (IsValidForTracksAdding()) {
        trackNumber = _segment.AddAudioTrack(0, channels, _nextNumber);
        if (trackNumber) {
            const auto track = dynamic_cast<mkvmuxer::AudioTrack*>(_segment.GetTrackByNumber(trackNumber));
            MS_ASSERT(track, "wrong type of newly added MKV audio track");
            _audioTracks[trackNumber] = track;
#ifdef CUES_NEW_ADDED_TRACK
            if (_videoTracks.empty()) {
                _segment.CuesTrack(trackNumber);
            }
#endif
            ++_nextNumber;
        }
    }
    return trackNumber;
}

uint64_t MkvBufferedWriter::AddVideoTrack(int32_t width, int32_t height)
{
    uint64_t trackNumber = 0ULL;
    if (IsValidForTracksAdding()) {
        trackNumber = _segment.AddVideoTrack(width, height, _nextNumber);
        if (trackNumber) {
            const auto track = dynamic_cast<mkvmuxer::VideoTrack*>(_segment.GetTrackByNumber(trackNumber));
            MS_ASSERT(track, "wrong type of newly added MKV video track");
            _videoTracks[trackNumber] = track;
#ifdef CUES_NEW_ADDED_TRACK
            if (1UL == _videoTracks.size()) {
                _segment.CuesTrack(trackNumber);
            }
#endif
            ++_nextNumber;
        }
    }
    return trackNumber;
}

bool MkvBufferedWriter::AddFrame(uint64_t trackNumber,
                                 const MediaFrame& mediaFrame,
                                 uint64_t mkvTimestamp)
{
    bool ok = EnqueueResult::Failure != EnqueueFrame(mediaFrame, mkvTimestamp, trackNumber);
    if (ok) {
        if (mediaFrame.IsAudio()) {
            if (!HasVideoTracks()) {
                ok = WriteFrames(_mkvAudioLastTimestamp);
            }
        }
        else { // video
            if (!HasAudioTracks()) {
                ok = WriteFrames(_mkvVideoLastTimestamp);
            }
            else {
                const auto ts = std::min(_mkvVideoLastTimestamp, _mkvAudioLastTimestamp);
                ok = WriteFrames(ts);
            }
        }
    }
    return ok;
}

bool MkvBufferedWriter::SetTrackCodec(uint64_t trackNumber, const char* codec)
{
    if (codec && std::strlen(codec)) {
        if (const auto track = _segment.GetTrackByNumber(trackNumber)) {
            track->set_codec_id(codec);
            return true;
        }
    }
    return false;
}

bool MkvBufferedWriter::SetTrackCodec(uint64_t trackNumber, const RtpCodecMimeType& mime)
{
    return SetTrackCodec(trackNumber, WebMCodecs::GetCodecId(mime));
}

bool MkvBufferedWriter::SetAudioSampleRate(uint64_t trackNumber, uint32_t sampleRate)
{
    if (const auto track = GetAudioTrack(trackNumber)) {
        if (sampleRate != track->sample_rate()) {
            track->set_sample_rate(sampleRate);
            // https://wiki.xiph.org/MatroskaOpus
            if (48000U == sampleRate && WebMCodecs::IsOpusCodec(track->codec_id())) {
                track->set_seek_pre_roll(80000000ULL);
            }
        }
        return true;
    }
    return false;
}

void MkvBufferedWriter::SetTrackSettings(uint64_t trackNumber,
                                         const AudioFrameConfig& config)
{
    if (const auto track = GetAudioTrack(trackNumber)) {
        track->set_channels(config.GetChannelCount());
        track->set_bit_depth(config.GetBitsPerSample());
        if (!SetCodecSpecific(track, config.GetCodecSpecificData())) {
            MS_ERROR("failed to setup of MKV writer audio codec "
                     "data for track #%" PRIu64, trackNumber);
        }
    }
}

void MkvBufferedWriter::SetTrackSettings(uint64_t trackNumber,
                                         const VideoFrameConfig& config)
{
    if (const auto track = GetVideoTrack(trackNumber)) {
        track->set_frame_rate(config.GetFrameRate());
        if (config.HasResolution()) {
            track->set_width(config.GetWidth());
            track->set_height(config.GetHeight());
            track->set_display_width(config.GetWidth());
            track->set_display_height(config.GetHeight());
        }
        else {
            MS_WARN_DEV("video resolution is not available or wrong for track #%" PRIu64, trackNumber);
        }
        if (!SetCodecSpecific(track, config.GetCodecSpecificData())) {
            MS_ERROR("failed to setup of MKV writer video codec "
                     "data for track #%" PRIu64, trackNumber);
        }
    }
}

bool MkvBufferedWriter::SetCodecSpecific(mkvmuxer::Track* track,
                                         const std::shared_ptr<const Buffer>& specific)
{
    if (track) {
        return !specific || track->SetCodecPrivate(specific->GetData(), specific->GetSize());
    }
    return true;
}

bool MkvBufferedWriter::IsValidForTracksAdding() const
{
    if (IsInitialized()) {
        MS_ASSERT(!HadWroteMedia(), "has wrotten bytes - reinitialization of MKV writer required");
        return true;
    }
    return false;
}

MkvBufferedWriter::EnqueueResult MkvBufferedWriter::EnqueueFrame(const MediaFrame& mediaFrame,
                                                                 uint64_t mkvTimestamp, uint64_t trackNumber)
{
    EnqueueResult result = EnqueueResult::Failure;
    if (IsInitialized()) {
        bool hasTrack = false;
        if (mediaFrame.IsAudio()) {
            hasTrack = _audioTracks.count(trackNumber) > 0UL;
        }
        else {
            hasTrack = _videoTracks.count(trackNumber) > 0UL;
        }
        if (hasTrack) {
            auto& mkvLastTimestamp = mediaFrame.IsAudio() ? _mkvAudioLastTimestamp : _mkvVideoLastTimestamp;
            if (mkvTimestamp >= mkvLastTimestamp) {
                if (!mediaFrame.IsAudio() || SetAudioSampleRate(trackNumber, mediaFrame.GetClockRate())) {
                    mkvmuxer::Frame frame;
                    if (frame.Init(std::make_shared<MkvFrameMemory>(mediaFrame.GetPayload()))) {
                        frame.set_track_number(trackNumber);
                        frame.set_timestamp(mkvTimestamp);
                        // is_key: -- always true for audio
                        frame.set_is_key(mediaFrame.IsKeyFrame() || mediaFrame.IsAudio());
                        _mkvFrames.push_back(std::move(frame));
                        result = EnqueueResult::Added;
                    }
                    else {
                        result = EnqueueResult::Dropped; // maybe empty
                    }
                    mkvLastTimestamp = mkvTimestamp;
                }
            }
            else {
                result = EnqueueResult::Dropped; // timestamp is too old
            }
        }
    }
    return result;
}

bool MkvBufferedWriter::WriteFrames(uint64_t mkvTimestamp)
{
    bool ok = true;
    if (!_mkvFrames.empty()) {
        if (HasAudioTracks() && HasVideoTracks() && _mkvFrames.size() > 1UL) {
            std::stable_sort(_mkvFrames.begin(), _mkvFrames.end(), CompareFrames);
        }
        size_t addedCount = 0UL;
        for (const auto& mkvFrame : _mkvFrames) {
            if (mkvFrame.timestamp() <= mkvTimestamp) {
                if (!HadWroteMedia() && !_startMediaSinkWriting) {
                    _sink->StartMediaWriting(_senderId);
                    _startMediaSinkWriting = true;
                }
                ok = _segment.AddGenericFrame(&mkvFrame);
                if (ok) {
                    MS_ASSERT(HadWroteMedia(), "incorrect writing");
                    ++addedCount;
                }
                else {
                    MS_ERROR("failed add MKV frame to segment");
                    break;
                }
            }
        }
        if (addedCount) {
            WriteMediaPayloadToSink();
            _mkvFrames.erase(_mkvFrames.begin(), _mkvFrames.begin() + addedCount);
        }
    }
    return ok;
}

mkvmuxer::AudioTrack* MkvBufferedWriter::GetAudioTrack(uint64_t trackNumber) const
{
    const auto it = _audioTracks.find(trackNumber);
    if (it != _audioTracks.end()) {
        return it->second;
    }
    return nullptr;
}

mkvmuxer::VideoTrack* MkvBufferedWriter::GetVideoTrack(uint64_t trackNumber) const
{
    const auto it = _videoTracks.find(trackNumber);
    if (it != _videoTracks.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Buffer> MkvBufferedWriter::TakeBuffer()
{
    std::shared_ptr<Buffer> buffer;
    if (_buffer) {
        buffer = std::make_shared<MkvBufferView>(std::move(_buffer), _bufferOffset);
        _bufferOffset = 0U;
    }
    return buffer;
}

void MkvBufferedWriter::WriteMediaPayloadToSink()
{
    if (const auto buffer = TakeBuffer()) {
        _sink->WriteMediaPayload(_senderId, buffer);
    }
}

mkvmuxer::int32 MkvBufferedWriter::Write(const void* buf, mkvmuxer::uint32 len)
{
    if (buf && len) {
        if (!_buffer) {
            const auto allocSize = std::max<size_t>(len, _bufferCapacity);
            _buffer = AllocateBuffer(allocSize, buf, len);
        }
        else {
            // buffer grows permanently until [MkvBufferedWriter::TakeBuffer] is not called
            _buffer = ReallocateBuffer(_bufferOffset + len, std::move(_buffer));
            if (_buffer) {
                std::memcpy(_buffer->GetData() + _bufferOffset, buf, len);
            }
        }
        if (_buffer) {
            _bufferOffset += len;
            _hadWroteMedia = true;
        }
    }
    return _buffer ? 0 : -1;
}

mkvmuxer::int64 MkvBufferedWriter::Position() const
{
    return static_cast<mkvmuxer::int64>(_bufferOffset);
}

MkvBufferedWriter::MkvFrameMemory::MkvFrameMemory(std::shared_ptr<const Buffer> data)
    : _data(std::move(data))
{
}

MkvBufferedWriter::MkvBufferView::MkvBufferView(std::shared_ptr<Buffer> buffer, size_t size)
    : _buffer(buffer)
    , _size(size)
{
    MS_ASSERT(_buffer->GetSize() >= _size, "incorrect buffer size");
}

}
