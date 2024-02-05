#define MS_CLASS "RTC::WebMSerializer"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "DepLibUV.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <mkvmuxer/mkvmuxer.h>

namespace {

using namespace RTC;

enum class EnqueueResult {
    Added,
    Dropped,
    Failure
};

template<typename T>
inline constexpr uint64_t ValueToNano(T value) {
    return value * 1000ULL * 1000ULL * 1000ULL;
}

inline bool IsOpus(RtpCodecMimeType::Subtype codec) {
    return RtpCodecMimeType::Subtype::OPUS == codec;
}

inline bool IsOpus(const RtpCodecMimeType& mime) {
    return IsOpus(mime.GetSubtype());
}

class MkvFrame
{
public:
    MkvFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
             uint64_t mkvTimestamp, int32_t trackNumber);
    MkvFrame(const MkvFrame&) = delete;
    MkvFrame(MkvFrame&&) = default;
    MkvFrame& operator = (const MkvFrame&) = delete;
    MkvFrame& operator = (MkvFrame&&) = default;
    uint64_t GetMkvTimestamp() const { return _mvkFrame ? _mvkFrame->timestamp() : 0ULL; }
    bool WriteToSegment(mkvmuxer::Segment& segment) const;
    bool IsValid() const { return _mediaFrame && _mvkFrame; }
    const std::shared_ptr<const MediaFrame>& GetMediaFrame() const { return _mediaFrame; }
private:
    static bool IsKeyFrame(const std::shared_ptr<const MediaFrame>& mediaFrame);
private:
    std::shared_ptr<const MediaFrame> _mediaFrame;
    std::unique_ptr<mkvmuxer::Frame> _mvkFrame;
};

}

namespace RTC
{

class WebMSerializer::BufferedWriter : private mkvmuxer::IMkvWriter
{
public:
    BufferedWriter(uint32_t ssrc, MediaSink* sink, uint32_t timeSliceMs, const char* app);
    ~BufferedWriter() final;
    bool IsInitialized() const { return _initialized; }
    bool HasAudioTracks() const { return _audioTracksCount > 0UL; }
    bool HasVideoTracks() const { return _videoTracksCount > 0UL; }
    bool AddFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                  uint64_t mkvTimestamp, int32_t trackNumber);
    bool AddAudioTrack(int32_t number);
    bool AddVideoTrack(int32_t number);
    bool SetTrackCodec(int32_t number, const char* codec);
    bool SetAudioSampleRate(int32_t number, uint32_t sampleRate, bool opusCodec);
    void SetTrackSettings(int32_t number, const std::shared_ptr<const AudioFrameConfig>& config);
    void SetTrackSettings(int32_t number, const std::shared_ptr<const VideoFrameConfig>& config);
private:
    static bool SetCodecSpecific(mkvmuxer::Track* track,
                                 const std::shared_ptr<const MemoryBuffer>& specific);
    void WriteMediaPayloadToSink(bool force = false);
    bool HasWroteMedia() const { return _wroteMedia; }
    void ReserveBuffer() { EnsureBufferSize(1024UL, true); }
    void EnsureBufferSize(size_t len, bool reserve);
    mkvmuxer::Track* GetTrack(int32_t number) const;
    bool IsValidForTracksAdding() const;
    EnqueueResult EnqueueFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                               uint64_t mkvTimestamp, int32_t trackNumber);
    bool WriteFrames(uint64_t mkvTimestamp);
    // Forces data output from |segment_| on the next frame if recording video,
    // and |min_data_output_interval_| was configured and has passed since the
    // last received video frame.
    void MaybeForceNewCluster();
    // impl. of mkvmuxer::IMkvWriter
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final;
    mkvmuxer::int64 Position() const final;
    mkvmuxer::int32 Position(mkvmuxer::int64 position) final;
    bool Seekable() const final { return true; }
    void ElementStartNotify(mkvmuxer::uint64 element_id, mkvmuxer::int64 position) final;
private:
    const uint32_t _ssrc;
    MediaSink* const _sink;
    const uint32_t _timeSliceMs;
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
    mkvmuxer::int64 _position = 0LL;
    uint64_t _sliceOriginTimestamp = 0ULL;
    uint64_t _lastDataOutputTimestamp = 0ULL;
};

WebMSerializer::WebMSerializer(uint32_t ssrc, uint32_t clockRate,
                               const RtpCodecMimeType& mime,
                               uint32_t timeSliceMs, const char* app)
    : MediaFrameSerializer(ssrc, clockRate, mime)
    , _timeSliceMs(timeSliceMs)
    , _app(app)
{
}

WebMSerializer::~WebMSerializer()
{
}

bool WebMSerializer::IsSupported(const RtpCodecMimeType& mimeType)
{
    return nullptr != GetCodecId(mimeType);
}

const char* WebMSerializer::GetCodecId(RtpCodecMimeType::Subtype codec)
{
    // EMBL header for H264 & H265 will be 'matroska' and 'webm' for other codec types
    // https://www.matroska.org/technical/codec_specs.html
    switch (codec) {
        case RtpCodecMimeType::Subtype::VP8:
            return mkvmuxer::Tracks::kVp8CodecId;
        case RtpCodecMimeType::Subtype::VP9:
            return mkvmuxer::Tracks::kVp9CodecId;
        case RtpCodecMimeType::Subtype::H264:
        case RtpCodecMimeType::Subtype::H264_SVC:
            return "V_MPEG4/ISO/AVC"; // matroska
        case RtpCodecMimeType::Subtype::H265:
            return "V_MPEGH/ISO/HEVC";
        case RtpCodecMimeType::Subtype::PCMA:
        case RtpCodecMimeType::Subtype::PCMU:
            return "A_PCM/FLOAT/IEEE";
        default:
            if (IsOpus(codec)) {
                return mkvmuxer::Tracks::kOpusCodecId;
            }
            break;
    }
    return nullptr;
}

const char* WebMSerializer::GetCodecId(const RtpCodecMimeType& mime)
{
    return GetCodecId(mime.GetSubtype());
}

bool WebMSerializer::AddSink(MediaSink* sink)
{
    if (sink && _sinks.end() == _sinks.find(sink)) {
        auto writer = std::make_unique<BufferedWriter>(GetSsrc(), sink, _timeSliceMs, _app);
        if (writer->IsInitialized()) {
            bool ok = false;
            switch (GetMimeType().GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    ok = writer->AddAudioTrack(_trackNumber);
                    if (ok) {
                        ok = writer->SetAudioSampleRate(_trackNumber, GetClockRate(), IsOpus(GetMimeType()));
                        if (!ok) {
                            MS_ERROR_STD("failed to set intial MKV audio sample rate");
                        }
                    }
                    else {
                        MS_ERROR_STD("failed to add MKV audio track");
                    }
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    ok = writer->AddVideoTrack(_trackNumber);
                    if (!ok) {
                        MS_ERROR_STD("failed to add MKV video track");
                    }
                    break;
            }
            if (ok) {
                ok = writer->SetTrackCodec(_trackNumber, GetCodecId(GetMimeType()));
                if (ok) {
                    _sinks[sink] = std::move(writer);
                }
                else {
                    MS_ERROR_STD("failed to set MKV codec ID: %s", GetMimeType().ToString().c_str());
                }
            }
            return ok;
        }
    }
    return false;
}

bool WebMSerializer::RemoveSink(MediaSink* sink)
{
    if (sink) {
        const auto it = _sinks.find(sink);
        if (it != _sinks.end()) {
            _sinks.erase(it);
            return true;
        }
    }
    return false;
}

void WebMSerializer::RemoveAllSinks()
{
    _sinks.clear();
}

bool WebMSerializer::HasSinks() const
{
    return !_sinks.empty();
}

size_t WebMSerializer::GetSinksCout() const
{
    return _sinks.size();
}

bool WebMSerializer::Push(const std::shared_ptr<const MediaFrame>& mediaFrame)
{
    if (mediaFrame && HasSinks() && IsAccepted(mediaFrame)) {
        const auto mkvTimestamp = UpdateTimeStamp(mediaFrame->GetTimestamp());
        for (auto it = _sinks.begin(); it != _sinks.end(); ++it) {
            switch (GetMimeType().GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    it->second->SetTrackSettings(_trackNumber, mediaFrame->GetAudioConfig());
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    it->second->SetTrackSettings(_trackNumber, mediaFrame->GetVideoConfig());
                    break;
            }
            if (!it->second->AddFrame(mediaFrame, mkvTimestamp, _trackNumber)) {
                const auto frameInfo = GetMediaFrameInfoString(mediaFrame, GetSsrc());
                MS_ERROR_STD("unable write frame to MKV data [%s]", frameInfo.c_str());
                return false;
            }
        }
        return true;
    }
    return false;
}


bool WebMSerializer::IsAccepted(const std::shared_ptr<const MediaFrame>& mediaFrame) const
{
    if (mediaFrame && mediaFrame->GetMimeType() == GetMimeType()) {
        const auto timestamp = mediaFrame->GetTimestamp();
        // special case if both timestamps are zero, for 1st initial frame
        return (0U == timestamp && 0U == _lastTimestamp) || timestamp > _lastTimestamp;
    }
    return false;
}

uint64_t WebMSerializer::UpdateTimeStamp(uint32_t timestamp)
{
    if (timestamp > _lastTimestamp) {
        if (_lastTimestamp) {
            _granule += timestamp - _lastTimestamp;
        }
        _lastTimestamp = timestamp;
    }
    return ValueToNano(_granule) / GetClockRate();
}

WebMSerializer::BufferedWriter::BufferedWriter(uint32_t ssrc, MediaSink* sink,
                                               uint32_t timeSliceMs, const char* app)
    : _ssrc(ssrc)
    , _sink(sink)
    , _timeSliceMs(timeSliceMs)
    , _initialized(_segment.Init(this))
{
    if (IsInitialized()) {
        ReserveBuffer();
        if (_sink->IsLiveMode()) {
            _segment.set_mode(mkvmuxer::Segment::kLive);
            _segment.set_estimate_file_duration(false);
            _segment.OutputCues(false);
        }
        else {
            _segment.set_mode(mkvmuxer::Segment::kFile);
            _segment.set_estimate_file_duration(true);
            _segment.OutputCues(true);
        }
        if (const auto segmentInfo = _segment.GetSegmentInfo()) {
            segmentInfo->set_writing_app(app);
            segmentInfo->set_muxing_app(app);
        }
    }
}

WebMSerializer::BufferedWriter::~BufferedWriter()
{
    if (IsInitialized()) {
        _segment.Finalize();
        WriteMediaPayloadToSink(true);
        if (HasWroteMedia()) {
            _sink->EndMediaWriting();
        }
    }
}

bool WebMSerializer::BufferedWriter::AddFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                              uint64_t mkvTimestamp,
                                              int32_t trackNumber)
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

bool WebMSerializer::BufferedWriter::AddAudioTrack(int32_t number)
{
    bool ok = false;
    if (IsValidForTracksAdding()) {
        if (!GetTrack(number)) {
            const auto refNumber = _segment.AddAudioTrack(0, 0, number);
            if (refNumber) {
                _tracksReference[number] = refNumber;
                ok = true;
                ++_audioTracksCount;
                if (0ULL == _videoTracksCount) {
                    _segment.CuesTrack(refNumber);
                }
            }
        }
        else {
            ok = true;
        }
    }
    return ok;
}

bool WebMSerializer::BufferedWriter::AddVideoTrack(int32_t number)
{
    bool ok = false;
    if (IsValidForTracksAdding()) {
        if (!GetTrack(number)) {
            const auto refNumber = _segment.AddVideoTrack(0, 0, number);
            if (refNumber) {
                _tracksReference[number] = refNumber;
                ok = true;
                if (0ULL == _videoTracksCount++) {
                    _segment.CuesTrack(refNumber);
                }
            }
        }
        else {
            ok = true;
        }
    }
    return ok;
}

bool WebMSerializer::BufferedWriter::SetTrackCodec(int32_t number, const char* codec)
{
    if (codec && std::strlen(codec)) {
        if (const auto track = GetTrack(number)) {
            track->set_codec_id(codec);
            return true;
        }
    }
    return false;
}

bool WebMSerializer::BufferedWriter::SetAudioSampleRate(int32_t number,
                                                        uint32_t sampleRate,
                                                        bool opusCodec)
{
    if (const auto track = static_cast<mkvmuxer::AudioTrack*>(GetTrack(number))) {
        track->set_sample_rate(sampleRate);
        // https://wiki.xiph.org/MatroskaOpus
        if (opusCodec && 48000U == sampleRate) {
            track->set_seek_pre_roll(80000000ULL);
        }
        return true;
    }
    return false;
}

void WebMSerializer::BufferedWriter::SetTrackSettings(int32_t number,
                                                      const std::shared_ptr<const AudioFrameConfig>& config)
{
    if (config) {
        if (const auto track = static_cast<mkvmuxer::AudioTrack*>(GetTrack(number))) {
            track->set_channels(config->GetChannelCount());
            track->set_bit_depth(config->GetBitsPerSample());
            if (!SetCodecSpecific(track, config->GetCodecSpecificData())) {
                MS_ERROR_STD("failed to setup of MKV writer audio codec data for track #%d", number);
            }
        }
    }
}

void WebMSerializer::BufferedWriter::SetTrackSettings(int32_t number,
                                                      const std::shared_ptr<const VideoFrameConfig>& config)
{
    if (config) {
        if (const auto track = static_cast<mkvmuxer::VideoTrack*>(GetTrack(number))) {
            track->set_frame_rate(config->GetFrameRate());
            if (config->HasResolution()) {
                track->set_width(config->GetWidth());
                track->set_height(config->GetHeight());
                track->set_display_width(config->GetWidth());
                track->set_display_height(config->GetHeight());
            }
            else {
                MS_WARN_DEV_STD("video resolution is not available or wrong for track #%d", number);
            }
            if (!SetCodecSpecific(track, config->GetCodecSpecificData())) {
                MS_ERROR_STD("failed to setup of MKV writer video codec data for track #%d", number);
            }
        }
    }
}

bool WebMSerializer::BufferedWriter::SetCodecSpecific(mkvmuxer::Track* track,
                                                      const std::shared_ptr<const MemoryBuffer>& specific)
{
    if (track) {
        return !specific || track->SetCodecPrivate(specific->GetData(), specific->GetSize());
    }
    return true;
}

void WebMSerializer::BufferedWriter::WriteMediaPayloadToSink(bool force)
{
    bool canWrite = force || 0U == _timeSliceMs;
    if (!canWrite) {
        const auto now = DepLibUV::GetTimeMs();
        canWrite = now > _sliceOriginTimestamp + _timeSliceMs;
        if (canWrite) {
            _sliceOriginTimestamp = now;
        }
    }
    if (canWrite) {
        if (const auto buffer = _buffer.Take()) {
            _position = 0LL;
            _sink->WriteMediaPayload(_ssrc, buffer);
            ReserveBuffer();
        }
    }
}

void WebMSerializer::BufferedWriter::EnsureBufferSize(size_t len, bool reserve)
{
    const auto size = static_cast<size_t>(_position) + len;
    if (reserve) {
        _buffer.Reserve(size);
    }
    else if (size > _buffer.GetSize()) {
        _buffer.Resize(size);
    }
}

mkvmuxer::Track* WebMSerializer::BufferedWriter::GetTrack(int32_t number) const
{
    if (IsInitialized()) {
        const auto it = _tracksReference.find(number);
        if (it != _tracksReference.end()) {
            return _segment.GetTrackByNumber(it->second);
        }
    }
    return nullptr;
}

bool WebMSerializer::BufferedWriter::IsValidForTracksAdding() const
{
    if (IsInitialized()) {
        MS_ASSERT(!HasWroteMedia(), "has wrotten bytes - reinitialization of MKV writer required");
        return true;
    }
    return false;
}

EnqueueResult WebMSerializer::BufferedWriter::EnqueueFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                                              uint64_t mkvTimestamp, int32_t trackNumber)
{
    EnqueueResult result = EnqueueResult::Failure;
    if (mediaFrame && IsInitialized()) {
        const auto it = _tracksReference.find(trackNumber);
        if (it != _tracksReference.end()) {
            auto& mkvLastTimestamp = mediaFrame->IsAudio() ? _mkvAudioLastTimestamp : _mkvVideoLastTimestamp;
            /*if (mkvTimestamp < mkvLastTimestamp) { // timestamp is too old
                mkvTimestamp = mkvLastTimestamp;
            }*/
            if (mkvTimestamp >= mkvLastTimestamp) {
                MkvFrame frame(mediaFrame, mkvTimestamp, trackNumber);
                if (frame.IsValid()) {
                    _mkvFrames.push_back(std::move(frame));
                    mkvLastTimestamp = mkvTimestamp;
                    result = EnqueueResult::Added;
                }
            }
            else {
                result = EnqueueResult::Dropped; // timestamp is too old
            }
        }
    }
    return result;
}

bool WebMSerializer::BufferedWriter::WriteFrames(uint64_t mkvTimestamp)
{
    bool ok = true;
    if (!_mkvFrames.empty()) {
        if (HasAudioTracks() && HasVideoTracks() && _mkvFrames.size() > 1UL) {
            std::stable_sort(_mkvFrames.begin(), _mkvFrames.end(),
                             [](const MkvFrame& a, const MkvFrame& b) {
                return a.GetMkvTimestamp() < b.GetMkvTimestamp();
            });
        }
        size_t addedCount = 0UL;
        for (const auto& mkvFrame : _mkvFrames) {
            if (mkvFrame.GetMkvTimestamp() <= mkvTimestamp) {
                if (!HasWroteMedia()) {
                    _sink->StartMediaWriting(false);
                    _sliceOriginTimestamp = DepLibUV::GetTimeMs();
                }
                if (mkvFrame.GetMediaFrame()->IsAudio()) {
                    MaybeForceNewCluster();
                }
                ok = mkvFrame.WriteToSegment(_segment);
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

void WebMSerializer::BufferedWriter::MaybeForceNewCluster()
{
    if (_videoTracksCount && _timeSliceMs && _lastDataOutputTimestamp
        && DepLibUV::GetTimeMs() - _lastDataOutputTimestamp >= _timeSliceMs) {
        _segment.ForceNewClusterOnNextFrame();
    }
}

mkvmuxer::int32 WebMSerializer::BufferedWriter::Write(const void* buf, mkvmuxer::uint32 len)
{
    if (buf && len) {
        EnsureBufferSize(len, false);
        std::memcpy(_buffer.GetData() + _position, buf, len);
        _position += len;
        _wroteMedia = true;
        _lastDataOutputTimestamp = DepLibUV::GetTimeMs();
        return 0;
    }
    return -1;
}

mkvmuxer::int64 WebMSerializer::BufferedWriter::Position() const
{
    return _position;
}

void WebMSerializer::BufferedWriter::ElementStartNotify(mkvmuxer::uint64 /*element_id*/,
                                                        mkvmuxer::int64 /*position*/)
{
    // Element start notification. Called whenever an element identifier is about
    // to be written to the stream. |element_id| is the element identifier, and
    // |position| is the location in the WebM stream where the first octet of the
    // element identifier will be written.
    // Note: the |MkvId| enumeration in webmids.hpp defines element values.
}

mkvmuxer::int32 WebMSerializer::BufferedWriter::Position(mkvmuxer::int64 position)
{
    if (position >= 0) {
        if (_position != position) {
            _position = position;
            EnsureBufferSize(0UL, true);
        }
        return 0;
    }
    return -1;
}

} // namespace RTC

namespace {

MkvFrame::MkvFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                   uint64_t mkvTimestamp, int32_t trackNumber)
    : _mediaFrame(mediaFrame)
{
    if (_mediaFrame && !_mediaFrame->IsEmpty()) {
        _mvkFrame = std::make_unique<mkvmuxer::Frame>();
        //_mvkFrame->Init(payload->GetData(), payload->GetSize());
        _mvkFrame->set_track_number(trackNumber);
        _mvkFrame->set_timestamp(mkvTimestamp);
        _mvkFrame->set_is_key(IsKeyFrame(mediaFrame));
    }
}

bool MkvFrame::WriteToSegment(mkvmuxer::Segment& segment) const
{
    bool ok = false;
    if (IsValid()) {
        const auto payload = _mediaFrame->GetPayload();
        if (payload && !payload->IsEmpty()) {
            ok = _mvkFrame->Init(payload->GetData(), payload->GetSize());
            if (ok) {
                ok = segment.AddGenericFrame(_mvkFrame.get());
            }
        }
    }
    return ok;
}

bool MkvFrame::IsKeyFrame(const std::shared_ptr<const MediaFrame>& mediaFrame)
{
    return mediaFrame && mediaFrame->IsKeyFrame();
}

}
