#define MS_CLASS "RTC::RtpWebMSerializer"
#include "RTC/MediaTranslate/WebM/RtpWebMSerializer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
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
    switch (codec) {
        case RtpCodecMimeType::Subtype::OPUS:
        //case RtpCodecMimeType::Subtype::MULTIOPUS:
            return true;
        default:
            break;
    }
    return false;
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

class RtpWebMSerializer::BufferedWriter : private mkvmuxer::IMkvWriter,
                                          private SimpleMemoryBuffer
{
public:
    BufferedWriter(const char* writingApp);
    ~BufferedWriter() final { Finalize(); }
    void Finalize();
    bool IsInitialized() const { return _initialized; }
    bool HasWroteMedia() const { return _wroteMedia; }
    bool HasAudioTracks() const { return _audioTracksCount > 0UL; }
    bool HasVideoTracks() const { return _videoTracksCount > 0UL; }
    void SetLiveMode(bool live);
    bool AddFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                  uint32_t ssrc, uint64_t mkvTimestamp, int32_t trackNumber,
                  RtpWebMSerializer* serializer);
    bool AddAudioTrack(int32_t number);
    bool AddVideoTrack(int32_t number);
    void SetAudioSampleRate(int32_t number, uint32_t sampleRate, bool opusCodec);
    void SetTrackSettings(int32_t number, const char* codec,
                          const std::shared_ptr<const AudioFrameConfig>& config);
    void SetTrackSettings(int32_t number, const char* codec,
                          const std::shared_ptr<const VideoFrameConfig>& config);
    void SetTrackSettings(const TrackInfo* trackInfo);
    std::shared_ptr<MemoryBuffer> TakeWrittenData();
private:
    static bool SetCodecSpecific(mkvmuxer::Track* track,
                                 const std::shared_ptr<const MemoryBuffer>& specific);
    mkvmuxer::Track* GetTrack(int32_t number) const;
    bool IsValidForTracksAdding() const;
    // 1kb buffer is enough for single OPUS frame
    // TODO: develop a strategy for optimal memory management for both audio & video (maybe mem pool)
    void ReserveBuffer() { Reserve(1024); }
    EnqueueResult EnqueueFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                               uint64_t mkvTimestamp, int32_t trackNumber);
    bool WriteFrames(uint32_t ssrc, uint64_t mkvTimestamp, RtpWebMSerializer* serializer);
    // impl. of mkvmuxer::IMkvWriter
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final;
    mkvmuxer::int64 Position() const final;
    mkvmuxer::int32 Position(mkvmuxer::int64 position) final;
    bool Seekable() const final { return true; }
    void ElementStartNotify(mkvmuxer::uint64 element_id, mkvmuxer::int64 position) final;
private:
    mkvmuxer::Segment _segment;
    const bool _initialized;
    bool _wroteMedia = false;
    absl::flat_hash_map<int32_t, uint64_t> _tracksReference;
    size_t _audioTracksCount = 0UL;
    size_t _videoTracksCount = 0UL;
    uint64_t _mkvVideoLastTimestamp = 0ULL;
    uint64_t _mkvAudioLastTimestamp = 0ULL;
    std::vector<MkvFrame> _mkvFrames;
};

class RtpWebMSerializer::TrackInfo
{
public:
    TrackInfo(int32_t number, bool audio, uint32_t clockRate);
    int32_t GetNumber() const { return _number; }
    uint32_t GetClockRate() const { return _clockRate; }
    bool IsAudio() const { return _audio; }
    bool IsAccepted(const std::shared_ptr<const MediaFrame>& mediaFrame) const;
    // return true if changed
    bool SetCodec(RtpCodecMimeType::Subtype codec);
    bool SetCodec(const RtpCodecMimeType& mime) { return SetCodec(mime.GetSubtype()); }
    const std::optional<RtpCodecMimeType::Subtype>& GetCodec() const { return _codec; }
    void ResetRtpTiming();
    uint64_t UpdateTimeStamp(uint32_t rtpTimestamp);
    void SetLatestConfig(const std::shared_ptr<const AudioFrameConfig>& config);
    void SetLatestConfig(const std::shared_ptr<const VideoFrameConfig>& config);
    const std::shared_ptr<const AudioFrameConfig>& GetLatestAudioConfig() const;
    const std::shared_ptr<const VideoFrameConfig>& GetLatestVideoConfig() const;
private:
    const int32_t _number;
    const bool _audio;
    const uint32_t _clockRate;
    std::optional<RtpCodecMimeType::Subtype> _codec;
    uint32_t _lastRtpTimestamp = 0ULL;
    uint64_t _granule = 0ULL;
    std::shared_ptr<const AudioFrameConfig> _latestAudioConfig;
    std::shared_ptr<const VideoFrameConfig> _latestVideoConfig;
};

RtpWebMSerializer::RtpWebMSerializer(const char* writingApp)
    : _writingApp(writingApp)
{
}

RtpWebMSerializer::~RtpWebMSerializer()
{
    DestroyWriter(false);
}

bool RtpWebMSerializer::IsSupported(const RtpCodecMimeType& mimeType)
{
    return nullptr != GetCodecId(mimeType);
}

const char* RtpWebMSerializer::GetCodecId(RtpCodecMimeType::Subtype codec)
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
        default:
            if (IsOpus(codec)) {
                return mkvmuxer::Tracks::kOpusCodecId;
            }
            break;
    }
    return nullptr;
}

const char* RtpWebMSerializer::GetCodecId(const RtpCodecMimeType& mime)
{
    return GetCodecId(mime.GetSubtype());
}

void RtpWebMSerializer::SetLiveMode(bool liveMode)
{
    RtpMediaFrameSerializer::SetLiveMode(liveMode);
    if (_liveMode != liveMode) {
        _liveMode = liveMode;
        if (_writer) {
            _writer->SetLiveMode(liveMode);
        }
    }
}

std::string_view RtpWebMSerializer::GetFileExtension(const RtpCodecMimeType&) const
{
    return "webm";
}

bool RtpWebMSerializer::AddAudio(uint32_t ssrc, uint32_t clockRate,
                                 RtpCodecMimeType::Subtype codec,
                                 const std::shared_ptr<const AudioFrameConfig>& config)
{
    if (ssrc) {
        const RtpCodecMimeType mime(RtpCodecMimeType::Type::AUDIO, codec);
        if(mime.IsAudioCodec()) {
            return AddMedia(ssrc, clockRate, mime, config);
        }
    }
    return false;
}

bool RtpWebMSerializer::AddVideo(uint32_t ssrc, uint32_t clockRate,
                                 RtpCodecMimeType::Subtype codec,
                                 const std::shared_ptr<const VideoFrameConfig>& config)
{
    if (ssrc) {
        const RtpCodecMimeType mime(RtpCodecMimeType::Type::VIDEO, codec);
        if(mime.IsVideoCodec()) {
            return AddMedia(ssrc, clockRate, mime, config);
        }
    }
    return false;
}

void RtpWebMSerializer::RemoveMedia(uint32_t ssrc)
{
    if (ssrc) {
        const auto it = _tracksInfo.find(ssrc);
        if (it != _tracksInfo.end()) {
            _tracksInfo.erase(it);
            if (_writer) {
                _pendingRestartMode = _writer->HasWroteMedia();
                DestroyWriter(false);
                if (!_tracksInfo.empty()) {
                    InitWriter();
                }
            }
        }
    }
}

bool RtpWebMSerializer::Push(uint32_t ssrc, const std::shared_ptr<const MediaFrame>& mediaFrame)
{
    if (mediaFrame && _writer && HasSinks()) {
        if (const auto trackInfo = GetTrackInfo(ssrc, mediaFrame)) {
            const auto mkvTimestamp = trackInfo->UpdateTimeStamp(mediaFrame->GetTimestamp());
            const auto trackNumber = trackInfo->GetNumber();
            if (!_writer->HasWroteMedia()) {
                StartMediaSinksStream(_pendingRestartMode);
                _pendingRestartMode = false;
            }
            const auto added = _writer->AddFrame(mediaFrame, ssrc, mkvTimestamp, trackNumber, this);
            if (!added) {
                const auto frameInfo = GetMediaFrameInfoString(mediaFrame, ssrc);
                MS_ERROR("unable write frame to MKV data [%s] to track #%d", frameInfo.c_str(), trackNumber);
                DestroyWriter(true);
            }
            return added;
        }
    }
    return false;
}

bool RtpWebMSerializer::IsCompatible(const RtpCodecMimeType& mimeType) const
{
    return IsSupported(mimeType);
}

RtpWebMSerializer::TrackInfo* RtpWebMSerializer::
    GetTrackInfo(uint32_t ssrc, const std::shared_ptr<const MediaFrame>& mediaFrame) const
{
    if (_writer && mediaFrame && !mediaFrame->IsEmpty()) {
        const auto& mime = mediaFrame->GetMimeType();
        if (IsSupported(mime)) {
            const auto it = _tracksInfo.find(ssrc);
            if (it != _tracksInfo.end() && it->second->IsAccepted(mediaFrame)) {
                const auto number = it->second->GetNumber();
                it->second->SetCodec(mime);
                const auto codec = GetCodecId(mime);
                if (it->second->IsAudio()) {
                    _writer->SetTrackSettings(number, codec, mediaFrame->GetAudioConfig());
                    it->second->SetLatestConfig(mediaFrame->GetAudioConfig());
                }
                else { // video
                    _writer->SetTrackSettings(number, codec, mediaFrame->GetVideoConfig());
                    it->second->SetLatestConfig(mediaFrame->GetVideoConfig());
                }
                return it->second.get();
            }
        }
    }
    return nullptr;
}

void RtpWebMSerializer::InitWriter()
{
    if (!_hasFailure) {
        auto writer = std::make_unique<BufferedWriter>(_writingApp);
        bool ok = writer->IsInitialized();
        if (ok) {
            for (auto it = _tracksInfo.begin(); it != _tracksInfo.end(); ++it) {
                const auto number = it->second->GetNumber();
                if (it->second->IsAudio()) {
                    ok = writer->AddAudioTrack(number);
                }
                else {
                    ok = writer->AddVideoTrack(number);
                }
                if (ok) {
                    writer->SetTrackSettings(it->second.get());
                }
                else {
                    break;
                }
            }
            if (ok) {
                for (auto it = _tracksInfo.begin(); it != _tracksInfo.end(); ++it) {
                    it->second->ResetRtpTiming();
                }
                _writer = std::move(writer);
                _writer->SetLiveMode(_liveMode);
            }
            else {
                MS_ERROR("failed to recreate of MKV writer tracks");
            }
        }
        else {
            MS_ERROR("failed to init of MKV writer segment");
        }
    }
}

void RtpWebMSerializer::DestroyWriter(bool failure)
{
    if (_writer) {
        _writer->Finalize();
        if (HasSinks()) {
            WriteMediaSinksPayload(_writer->TakeWrittenData());
        }
        if (_writer->HasWroteMedia()) {
            EndMediaSinksStream(failure);
        }
        if (failure) {
            _hasFailure = true;
        }
        _writer.reset();
    }
}

template<class TConfig>
bool RtpWebMSerializer::AddMedia(uint32_t ssrc, uint32_t clockRate,
                                 const RtpCodecMimeType& mime,
                                 const std::shared_ptr<TConfig>& config)
{
    bool registered = false;
    if (!_hasFailure && ssrc) {
        const auto it = _tracksInfo.find(ssrc);
        if (it == _tracksInfo.end()) {
            if (!_writer) {
                InitWriter();
            }
            else if(_writer->HasWroteMedia()) {
                DestroyWriter(false);
                InitWriter();
                _pendingRestartMode = true;
            }
            if (_writer) {
                bool added = false;
                const auto number = static_cast<int32_t>(_tracksInfo.size()) + 1;
                if (RtpCodecMimeType::Type::AUDIO == mime.GetType()) {
                    added = _writer->AddAudioTrack(number);
                }
                else if (RtpCodecMimeType::Type::VIDEO == mime.GetType()) {
                    added = _writer->AddVideoTrack(number);
                }
                if (added) {
                    _writer->SetTrackSettings(number, GetCodecId(mime), config);
                    const auto audio = RtpCodecMimeType::Type::AUDIO == mime.GetType();
                    auto trackInfo = std::make_unique<TrackInfo>(number, audio, clockRate);
                    if (audio) {
                        _writer->SetAudioSampleRate(number, clockRate, IsOpus(mime));
                    }
                    trackInfo->SetCodec(mime);
                    trackInfo->SetLatestConfig(config);
                    _tracksInfo[ssrc] = std::move(trackInfo);
                    registered = true;
                }
                else {
                    MS_ERROR("unable to create MKV writer track, [SSRC = %du]", ssrc);
                    DestroyWriter(true);
                }
            }
        }
        else {
            registered = true; // already registered
        }
    }
    return registered;
}

RtpWebMSerializer::BufferedWriter::BufferedWriter(const char* writingApp)
    : _initialized(_segment.Init(this))
{
    if (IsInitialized()) {
        if (const auto segmentInfo = _segment.GetSegmentInfo()) {
            segmentInfo->set_writing_app(writingApp);
        }
        ReserveBuffer();
    }
}

void RtpWebMSerializer::BufferedWriter::Finalize()
{
    if (IsInitialized()) {
        _segment.Finalize();
    }
}

void RtpWebMSerializer::BufferedWriter::SetLiveMode(bool live)
{
    if (_initialized) {
        if (live) {
            _segment.set_mode(mkvmuxer::Segment::kLive);
            _segment.set_estimate_file_duration(false);
            _segment.OutputCues(false);
        }
        else {
            _segment.set_mode(mkvmuxer::Segment::kFile);
            _segment.set_estimate_file_duration(true);
            _segment.OutputCues(true);
        }
    }
}

bool RtpWebMSerializer::BufferedWriter::AddFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                                 uint32_t ssrc, uint64_t mkvTimestamp,
                                                 int32_t trackNumber,
                                                 RtpWebMSerializer* serializer)
{
    const auto result = EnqueueFrame(mediaFrame, mkvTimestamp, trackNumber);
    bool ok = EnqueueResult::Failure != result;
    if (ok) {
        if (EnqueueResult::Added == result) {
            if (mediaFrame->IsAudio()) {
                if (!HasVideoTracks()) {
                    ok = WriteFrames(ssrc, _mkvAudioLastTimestamp, serializer);
                }
            }
            else { // video
                if (!HasAudioTracks()) {
                    ok = WriteFrames(ssrc, _mkvVideoLastTimestamp, serializer);
                }
                else {
                    const auto ts = std::min(_mkvVideoLastTimestamp, _mkvAudioLastTimestamp);
                    ok = WriteFrames(ssrc, ts, serializer);
                }
            }
        }
    }
    return ok;
}

bool RtpWebMSerializer::BufferedWriter::AddAudioTrack(int32_t number)
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

bool RtpWebMSerializer::BufferedWriter::AddVideoTrack(int32_t number)
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

void RtpWebMSerializer::BufferedWriter::SetAudioSampleRate(int32_t number,
                                                           uint32_t sampleRate,
                                                           bool opusCodec)
{
    if (const auto track = static_cast<mkvmuxer::AudioTrack*>(GetTrack(number))) {
        track->set_sample_rate(sampleRate);
        // https://wiki.xiph.org/MatroskaOpus
        if (opusCodec && 48000U == sampleRate) {
            track->set_seek_pre_roll(80000000ULL);
        }
    }
}

void RtpWebMSerializer::BufferedWriter::SetTrackSettings(int32_t number,
                                                         const char* codec,
                                                         const std::shared_ptr<const AudioFrameConfig>& config)
{
    if (const auto track = static_cast<mkvmuxer::AudioTrack*>(GetTrack(number))) {
        track->set_codec_id(codec);
        if (config) {
            track->set_channels(config->GetChannelCount());
            track->set_bit_depth(config->GetBitsPerSample());
            if (!SetCodecSpecific(track, config->GetCodecSpecificData())) {
                MS_ERROR("failed to setup of MKV writer audio codec data for track #%d", number);
            }
        }
    }
}

void RtpWebMSerializer::BufferedWriter::SetTrackSettings(int32_t number,
                                                         const char* codec,
                                                         const std::shared_ptr<const VideoFrameConfig>& config)
{
    if (const auto track = static_cast<mkvmuxer::VideoTrack*>(GetTrack(number))) {
        track->set_codec_id(codec);
        if (config) {
            track->set_frame_rate(config->GetFrameRate());
            if (config->HasResolution()) {
                track->set_width(config->GetWidth());
                track->set_height(config->GetHeight());
                track->set_display_width(config->GetWidth());
                track->set_display_height(config->GetHeight());
            }
            else {
                MS_WARN_DEV("video resolution is not available or wrong for track #%d", number);
            }
            if (!SetCodecSpecific(track, config->GetCodecSpecificData())) {
                MS_ERROR("failed to setup of MKV writer video codec data for track #%d", number);
            }
        }
    }
}

void RtpWebMSerializer::BufferedWriter::SetTrackSettings(const TrackInfo* trackInfo)
{
    if (trackInfo) {
        if (const auto& codec = trackInfo->GetCodec()) {
            if (const auto codecId = GetCodecId(codec.value())) {
                if (trackInfo->IsAudio()) {
                    SetTrackSettings(trackInfo->GetNumber(), codecId, trackInfo->GetLatestAudioConfig());
                    SetAudioSampleRate(trackInfo->GetNumber(), trackInfo->GetClockRate(),
                                       IsOpus(codec.value()));
                }
                else {
                    SetTrackSettings(trackInfo->GetNumber(), codecId, trackInfo->GetLatestVideoConfig());
                }
            }
        }
    }
}

std::shared_ptr<MemoryBuffer> RtpWebMSerializer::BufferedWriter::TakeWrittenData()
{
    if (const auto buffer = Take()) {
        ReserveBuffer();
        return buffer;
    }
    return nullptr;
}


bool RtpWebMSerializer::BufferedWriter::SetCodecSpecific(mkvmuxer::Track* track,
                                                         const std::shared_ptr<const MemoryBuffer>& specific)
{
    if (track) {
        return !specific || track->SetCodecPrivate(specific->GetData(), specific->GetSize());
    }
    return true;
}

mkvmuxer::Track* RtpWebMSerializer::BufferedWriter::GetTrack(int32_t number) const
{
    if (IsInitialized()) {
        const auto it = _tracksReference.find(number);
        if (it != _tracksReference.end()) {
            return _segment.GetTrackByNumber(it->second);
        }
    }
    return nullptr;
}

bool RtpWebMSerializer::BufferedWriter::IsValidForTracksAdding() const
{
    if (IsInitialized()) {
        MS_ASSERT(!HasWroteMedia(), "has wrotten bytes - reinitialization of MKV writer required");
        return true;
    }
    return false;
}

EnqueueResult RtpWebMSerializer::BufferedWriter::EnqueueFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
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

bool RtpWebMSerializer::BufferedWriter::WriteFrames(uint32_t ssrc, uint64_t mkvTimestamp,
                                                    RtpWebMSerializer* serializer)
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
                if (serializer) {
                    serializer->BeginWriteMediaSinksPayload(ssrc);
                }
                ok = mkvFrame.WriteToSegment(_segment);
                if (serializer) {
                    if (ok) {
                        serializer->WriteMediaSinksPayload(TakeWrittenData());
                    }
                    serializer->EndWriteMediaSinksPayload(ssrc, ok);
                }
                if (ok) {
                    ++addedCount;
                }
                else {
                    break;
                }
            }
        }
        if (addedCount) {
            _mkvFrames.erase(_mkvFrames.begin(), _mkvFrames.begin() + addedCount);
        }
    }
    return ok;
}

mkvmuxer::int32 RtpWebMSerializer::BufferedWriter::Write(const void* buf, mkvmuxer::uint32 len)
{
    if (Append(buf, len)) {
        _wroteMedia = true;
        return 0;
    }
    return 1;
}

mkvmuxer::int64 RtpWebMSerializer::BufferedWriter::Position() const
{
    return static_cast<mkvmuxer::int64>(GetSize());
}

void RtpWebMSerializer::BufferedWriter::ElementStartNotify(mkvmuxer::uint64 /*element_id*/,
                                                           mkvmuxer::int64 /*position*/)
{
    // Element start notification. Called whenever an element identifier is about
    // to be written to the stream. |element_id| is the element identifier, and
    // |position| is the location in the WebM stream where the first octet of the
    // element identifier will be written.
    // Note: the |MkvId| enumeration in webmids.hpp defines element values.
}

mkvmuxer::int32 RtpWebMSerializer::BufferedWriter::Position(mkvmuxer::int64 position)
{
    Resize(position);
    return 0;
}

RtpWebMSerializer::TrackInfo::TrackInfo(int32_t number, bool audio, uint32_t clockRate)
    : _number(number)
    , _audio(audio)
    , _clockRate(clockRate)
{
    MS_ASSERT(_clockRate, "sample rate must be greater than zero");
}

bool RtpWebMSerializer::TrackInfo::IsAccepted(const std::shared_ptr<const MediaFrame>& mediaFrame) const
{
    return mediaFrame && mediaFrame->GetTimestamp() > _lastRtpTimestamp;
}

bool RtpWebMSerializer::TrackInfo::SetCodec(RtpCodecMimeType::Subtype codec)
{
    if (codec != _codec) {
        _codec = codec;
        return true;
    }
    return false;
}

void RtpWebMSerializer::TrackInfo::ResetRtpTiming()
{
    _lastRtpTimestamp = 0U;
    _granule = 0ULL;
}

uint64_t RtpWebMSerializer::TrackInfo::UpdateTimeStamp(uint32_t rtpTimestamp)
{
    if (rtpTimestamp > _lastRtpTimestamp) {
        if (_lastRtpTimestamp) {
            _granule += rtpTimestamp - _lastRtpTimestamp;
        }
        _lastRtpTimestamp = rtpTimestamp;
    }
    return ValueToNano(_granule) / GetClockRate();
}

void RtpWebMSerializer::TrackInfo::SetLatestConfig(const std::shared_ptr<const AudioFrameConfig>& config)
{
    if (config) {
        MS_ASSERT(IsAudio(), "set incorrect config for audio track");
        _latestAudioConfig = config;
    }
}

void RtpWebMSerializer::TrackInfo::SetLatestConfig(const std::shared_ptr<const VideoFrameConfig>& config)
{
    if (config) {
        MS_ASSERT(!IsAudio(), "set incorrect config for video track");
        _latestVideoConfig = config;
    }
}

const std::shared_ptr<const AudioFrameConfig>& RtpWebMSerializer::TrackInfo::GetLatestAudioConfig() const
{
    MS_ASSERT(IsAudio(), "get incorrect config for audio track");
    return _latestAudioConfig;
}

const std::shared_ptr<const VideoFrameConfig>& RtpWebMSerializer::TrackInfo::GetLatestVideoConfig() const
{
    MS_ASSERT(!IsAudio(), "get incorrect config for video track");
    return _latestVideoConfig;
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
    return mediaFrame && (mediaFrame->IsKeyFrame() || mediaFrame->IsAudio());
}

}
