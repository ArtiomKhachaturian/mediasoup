#define MS_CLASS "RTC::MkvBufferedWriter"
#include "RTC/MediaTranslate/WebM/MkvBufferedWriter.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "Logger.hpp"
#include <algorithm> // for stable_sort

//#define CUES_NEW_ADDED_TRACK

namespace RTC
{

enum class MkvBufferedWriter::EnqueueResult {
    Added,
    Dropped,
    Failure
};

class MkvBufferedWriter::MkvFrame
{
public:
    MkvFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
             uint64_t mkvTimestamp, uint64_t trackNumber);
    MkvFrame(const MkvFrame&) = delete;
    MkvFrame(MkvFrame&&) = default;
    MkvFrame& operator = (const MkvFrame&) = delete;
    MkvFrame& operator = (MkvFrame&&) = default;
    uint64_t GetMkvTimestamp() const { return _mvkFrame ? _mvkFrame->timestamp() : 0ULL; }
    bool IsValid() const { return _mediaFrame && _mvkFrame; }
    const std::shared_ptr<const MediaFrame>& GetMediaFrame() const { return _mediaFrame; }
    bool InitPayload(uint32_t ssrc);
    operator const mkvmuxer::Frame* () const { return _mvkFrame.get(); }
    // for ordering by timestamps
    bool operator < (const MkvFrame& other) const { return GetMkvTimestamp() < other.GetMkvTimestamp(); }
    bool operator > (const MkvFrame& other) const { return GetMkvTimestamp() > other.GetMkvTimestamp(); }
private:
    static bool IsKeyFrame(const std::shared_ptr<const MediaFrame>& mediaFrame);
private:
    std::shared_ptr<const MediaFrame> _mediaFrame;
    std::unique_ptr<mkvmuxer::Frame> _mvkFrame;
};

MkvBufferedWriter::MkvBufferedWriter(uint32_t ssrc, MediaSink* sink, const char* app)
    : _ssrc(ssrc)
    , _sink(sink)
    , _initialized(_segment.Init(this))
{
    if (IsInitialized()) {
        ReserveBuffer();
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
            _sink->EndMediaWriting(_ssrc);
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
                                 const std::shared_ptr<const MediaFrame>& mediaFrame,
                                 uint64_t mkvTimestamp)
{
    const auto result = EnqueueFrame(mediaFrame, mkvTimestamp, trackNumber);
    bool ok = EnqueueResult::Failure != result;
    if (ok) {
        if (EnqueueResult::Added == result) {
            if (mediaFrame->IsAudio()) {
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
                                         const std::shared_ptr<const AudioFrameConfig>& config)
{
    if (config) {
        if (const auto track = GetAudioTrack(trackNumber)) {
            track->set_channels(config->GetChannelCount());
            track->set_bit_depth(config->GetBitsPerSample());
            if (!SetCodecSpecific(track, config->GetCodecSpecificData())) {
                MS_ERROR_STD("failed to setup of MKV writer audio codec data for track #%llu",
                             trackNumber);
            }
        }
    }
}

void MkvBufferedWriter::SetTrackSettings(uint64_t trackNumber,
                                         const std::shared_ptr<const VideoFrameConfig>& config)
{
    if (config) {
        if (const auto track = GetVideoTrack(trackNumber)) {
            track->set_frame_rate(config->GetFrameRate());
            if (config->HasResolution()) {
                track->set_width(config->GetWidth());
                track->set_height(config->GetHeight());
                track->set_display_width(config->GetWidth());
                track->set_display_height(config->GetHeight());
            }
            else {
                MS_WARN_DEV_STD("video resolution is not available or wrong for track #%llu",
                                trackNumber);
            }
            if (!SetCodecSpecific(track, config->GetCodecSpecificData())) {
                MS_ERROR_STD("failed to setup of MKV writer video codec data for track #%llu",
                             trackNumber);
            }
        }
    }
}

bool MkvBufferedWriter::SetCodecSpecific(mkvmuxer::Track* track,
                                         const std::shared_ptr<const MemoryBuffer>& specific)
{
    if (track) {
        return !specific || track->SetCodecPrivate(specific->GetData(), specific->GetSize());
    }
    return true;
}

void MkvBufferedWriter::WriteMediaPayloadToSink()
{
    if (const auto buffer = _buffer.Take()) {
        _sink->WriteMediaPayload(_ssrc, buffer);
        ReserveBuffer();
    }
}

bool MkvBufferedWriter::IsValidForTracksAdding() const
{
    if (IsInitialized()) {
        MS_ASSERT(!HasWroteMedia(), "has wrotten bytes - reinitialization of MKV writer required");
        return true;
    }
    return false;
}

MkvBufferedWriter::EnqueueResult MkvBufferedWriter::EnqueueFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                                                 uint64_t mkvTimestamp, uint64_t trackNumber)
{
    EnqueueResult result = EnqueueResult::Failure;
    if (mediaFrame && IsInitialized()) {
        bool hasTrack = false;
        if (mediaFrame->IsAudio()) {
            hasTrack = _audioTracks.count(trackNumber) > 0UL;
        }
        else {
            hasTrack = _videoTracks.count(trackNumber) > 0UL;
        }
        if (hasTrack) {
            auto& mkvLastTimestamp = mediaFrame->IsAudio() ? _mkvAudioLastTimestamp : _mkvVideoLastTimestamp;
            /*if (mkvTimestamp < mkvLastTimestamp) { // timestamp is too old
                mkvTimestamp = mkvLastTimestamp;
             }*/
            if (mkvTimestamp >= mkvLastTimestamp) {
                if (!mediaFrame->IsAudio() || SetAudioSampleRate(trackNumber, mediaFrame->GetClockRate())) {
                    MkvFrame frame(mediaFrame, mkvTimestamp, trackNumber);
                    if (frame.IsValid()) {
                        result = EnqueueResult::Added;
                        _mkvFrames.push_back(std::move(frame));
                        mkvLastTimestamp = mkvTimestamp;
                    }
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
            std::stable_sort(_mkvFrames.begin(), _mkvFrames.end());
        }
        size_t addedCount = 0UL;
        for (auto& mkvFrame : _mkvFrames) {
            if (mkvFrame.GetMkvTimestamp() <= mkvTimestamp) {
                if (!HasWroteMedia() && !_startMediaSinkWriting) {
                    _sink->StartMediaWriting(_ssrc);
                    _startMediaSinkWriting = true;
                }
                ok = mkvFrame.InitPayload(_ssrc);
                if (ok) {
                    ok = _segment.AddGenericFrame(mkvFrame);
                    if (!ok) {
                        MS_ERROR_STD("failed add MKV frame to segment for %u SSRC", _ssrc);
                    }
                }
                if (ok) {
                    MS_ASSERT(HasWroteMedia(), "incorrect writing");
                    ++addedCount;
                }
                else {
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

mkvmuxer::int32 MkvBufferedWriter::Write(const void* buf, mkvmuxer::uint32 len)
{
    if (_buffer.Append(buf, len)) {
        _wroteMedia = true;
        return 0;
    }
    return -1;
}

mkvmuxer::int64 MkvBufferedWriter::Position() const
{
    return static_cast<mkvmuxer::int64>(_buffer.GetSize());
}

MkvBufferedWriter::MkvFrame::MkvFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                      uint64_t mkvTimestamp, uint64_t trackNumber)
    : _mediaFrame(mediaFrame)
{
    if (_mediaFrame && !_mediaFrame->IsEmpty()) {
        _mvkFrame = std::make_unique<mkvmuxer::Frame>();
        _mvkFrame->set_track_number(trackNumber);
        _mvkFrame->set_timestamp(mkvTimestamp);
        _mvkFrame->set_is_key(IsKeyFrame(mediaFrame));
    }
}

bool MkvBufferedWriter::MkvFrame::InitPayload(uint32_t ssrc)
{
    if (_mvkFrame) {
        if (!_mvkFrame->IsValid()) {
            const auto payload = _mediaFrame->GetPayload();
            if (payload && !payload->IsEmpty()) {
                if (!_mvkFrame->Init(payload->GetData(), payload->GetSize())) {
                    MS_ERROR_STD("failed to init MKV frame for %u SSRC", ssrc);
                }
            }
            else {
                MS_ERROR_STD("no payload for initialization of MKV frame for %u SSRC", ssrc);
            }
        }
        return _mvkFrame->IsValid();
    }
    return false;
}

bool MkvBufferedWriter::MkvFrame::IsKeyFrame(const std::shared_ptr<const MediaFrame>& mediaFrame)
{
    // is_key: -- always true for audio
    return mediaFrame && (mediaFrame->IsKeyFrame() || mediaFrame->IsAudio());
}

}
