#define MS_CLASS "RTC::WebMSerializer"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
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

}

namespace RTC
{

class WebMSerializer::BufferedWriter : private mkvmuxer::IMkvWriter
{
public:
    BufferedWriter(uint32_t ssrc, MediaSink* sink, const char* app);
    ~BufferedWriter() final;
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
    mkvmuxer::int32 Position(mkvmuxer::int64 position) final;
    bool Seekable() const final;
    void ElementStartNotify(mkvmuxer::uint64 element_id, mkvmuxer::int64 position) final;
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

WebMSerializer::WebMSerializer(uint32_t ssrc, uint32_t clockRate,
                               const RtpCodecMimeType& mime, const char* app)
    : MediaFrameSerializer(ssrc, clockRate, mime)
    , _app(app)
{
    _sinks.reserve(2UL);
}

WebMSerializer::~WebMSerializer()
{
    WebMSerializer::RemoveAllSinks();
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
    bool added = false;
    if (sink) {
        added = _sinks.end() != _sinks.find(sink);
        if (!added) {
            if (auto writer = CreateWriter(sink)) {
                _sinks[sink] = std::move(writer);
                added = true;
            }
        }
    }
    return added;
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
    bool ok = false;
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
            ok = it->second->AddFrame(mediaFrame, mkvTimestamp, _trackNumber);
            if (!ok) {
                const auto frameInfo = GetMediaFrameInfoString(mediaFrame, GetSsrc());
                MS_ERROR_STD("unable write frame to MKV data [%s]", frameInfo.c_str());
                break;
            }
        }
    }
    return ok;
}

std::unique_ptr<WebMSerializer::BufferedWriter> WebMSerializer::CreateWriter(MediaSink* sink) const
{
    if (sink) {
        const auto& mime = GetMimeType();
        const auto ssrc = GetSsrc();
        auto writer = std::make_unique<BufferedWriter>(ssrc, sink, _app);
        if (writer->IsInitialized()) {
            bool ok = false;
            const auto clockRate = GetClockRate();
            switch (mime.GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    ok = writer->AddAudioTrack(_trackNumber, clockRate);
                    if (ok) {
                        ok = writer->SetAudioSampleRate(_trackNumber, clockRate, IsOpus(mime));
                        if (!ok) {
                            MS_ERROR_STD("failed to set intial MKV audio sample rate for %s",
                                         GetStreamInfoString(mime, ssrc).c_str());
                        }
                    }
                    else {
                        MS_ERROR_STD("failed to add MKV audio track for %s",
                                     GetStreamInfoString(mime, ssrc).c_str());
                    }
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    ok = writer->AddVideoTrack(_trackNumber);
                    if (!ok) {
                        MS_ERROR_STD("failed to add MKV video track for %s",
                                     GetStreamInfoString(mime, ssrc).c_str());
                    }
                    break;
            }
            if (ok) {
                ok = writer->SetTrackCodec(_trackNumber, GetCodecId(mime));
                if (!ok) {
                    MS_ERROR_STD("failed to set MKV codec for %s",
                                 GetStreamInfoString(mime, ssrc).c_str());
                    writer.reset();
                }
                return writer;
            }
        }
        else {
            MS_ERROR_STD("failed to initialize MKV writer for %s",
                         GetStreamInfoString(mime, ssrc).c_str());
        }
    }
    return nullptr;
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
                                               const char* app)
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

WebMSerializer::BufferedWriter::~BufferedWriter()
{
    if (IsInitialized()) {
        _segment.Finalize();
        WriteMediaPayloadToSink();
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

bool WebMSerializer::BufferedWriter::AddAudioTrack(int32_t number, int32_t sampleRate,
                                                   int32_t channels)
{
    bool ok = false;
    if (IsValidForTracksAdding()) {
        if (!GetTrack(number)) {
            const auto refNumber = _segment.AddAudioTrack(sampleRate, channels, number);
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

bool WebMSerializer::BufferedWriter::AddVideoTrack(int32_t number, int32_t width, int32_t height)
{
    bool ok = false;
    if (IsValidForTracksAdding()) {
        if (!GetTrack(number)) {
            const auto refNumber = _segment.AddVideoTrack(width, height, number);
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

void WebMSerializer::BufferedWriter::WriteMediaPayloadToSink()
{
    if (const auto buffer = _buffer.Take()) {
        _sink->WriteMediaPayload(buffer);
        ReserveBuffer();
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
            std::stable_sort(_mkvFrames.begin(), _mkvFrames.end());
        }
        size_t addedCount = 0UL;
        for (auto& mkvFrame : _mkvFrames) {
            if (mkvFrame.GetMkvTimestamp() <= mkvTimestamp) {
                if (!HasWroteMedia()) {
                    _sink->StartMediaWriting(_ssrc);
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

mkvmuxer::int32 WebMSerializer::BufferedWriter::Write(const void* buf, mkvmuxer::uint32 len)
{
    if (_buffer.Append(buf, len)) {
        _wroteMedia = true;
        return 0;
    }
    return -1;
}

mkvmuxer::int64 WebMSerializer::BufferedWriter::Position() const
{
    return static_cast<mkvmuxer::int64>(_buffer.GetSize());
}

mkvmuxer::int32 WebMSerializer::BufferedWriter::Position(mkvmuxer::int64 /*position*/)
{
    return -1;
}

bool WebMSerializer::BufferedWriter::Seekable() const
{
    return false;
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

} // namespace RTC

namespace {

MkvFrame::MkvFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                   uint64_t mkvTimestamp, int32_t trackNumber)
    : _mediaFrame(mediaFrame)
{
    if (_mediaFrame && !_mediaFrame->IsEmpty()) {
        _mvkFrame = std::make_unique<mkvmuxer::Frame>();
        _mvkFrame->set_track_number(trackNumber);
        _mvkFrame->set_timestamp(mkvTimestamp);
        _mvkFrame->set_is_key(IsKeyFrame(mediaFrame));
    }
}

bool MkvFrame::InitPayload(uint32_t ssrc)
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

bool MkvFrame::IsKeyFrame(const std::shared_ptr<const MediaFrame>& mediaFrame)
{
    // is_key: -- always true for audio
    return mediaFrame && (mediaFrame->IsKeyFrame() || mediaFrame->IsAudio());
}

}
