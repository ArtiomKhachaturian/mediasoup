#define MS_CLASS "RTC::RtpWebMSerializer"
#include "RTC/MediaTranslate/RtpWebMSerializer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <mkvmuxer/mkvmuxer.h>

namespace {

using namespace RTC;

inline uint64_t MilliToNano(uint64_t milli) {
    return milli * 1000ULL * 1000ULL * 1000ULL;
}

inline bool IsOpusAudioCodec(RtpCodecMimeType::Subtype codec) {
    switch (codec) {
        case RtpCodecMimeType::Subtype::OPUS:
        case RtpCodecMimeType::Subtype::MULTIOPUS:
            // https://en.wikipedia.org/wiki/SILK
        case RtpCodecMimeType::Subtype::SILK: // needs to be tested
            return true;
        default:
            break;
    }
    return false;
}

inline bool IsOpusAudioCodec(const RtpCodecMimeType& mime) {
    return IsOpusAudioCodec(mime.GetSubtype());
}

inline static const char* GetCodecId(RtpCodecMimeType::Subtype codec) {
    switch (codec) {
        case RtpCodecMimeType::Subtype::VP8:
            return mkvmuxer::Tracks::kVp8CodecId;
        case RtpCodecMimeType::Subtype::VP9:
            return mkvmuxer::Tracks::kVp9CodecId;
        default:
            if (IsOpusAudioCodec(codec)) {
                return mkvmuxer::Tracks::kOpusCodecId;
            }
            break;
    }
    return nullptr;
}

inline static const char* GetCodecId(const RtpCodecMimeType& mime) {
    return GetCodecId(mime.GetSubtype());
}

/*class MkvFrame
{
private:
    std::shared_ptr<const MemoryBuffer> _data;
    bool _isKeyFrame;
    uint64_t _timeStampNano;
};*/

}

namespace RTC
{

class RtpWebMSerializer::BufferedWriter : private mkvmuxer::IMkvWriter,
                                          private SimpleMemoryBuffer
{
public:
    BufferedWriter();
    ~BufferedWriter() final { Finalize(); }
    void Finalize();
    bool IsInitialized() const { return _initialized; }
    bool HasWroteMedia() const { return _wroteMedia; }
    void SetLiveMode(bool live);
    bool AddFrame(const std::shared_ptr<const MemoryBuffer>& data, int32_t number,
                  uint64_t timeCode, bool isKeyFrame);
    bool AddAudioTrack(int32_t number);
    bool AddVideoTrack(int32_t number);
    void SetAudioSampleRate(int32_t number, uint32_t sampleRate, bool opusCodec);
    void SetTrackSettings(int32_t number, const char* codec,
                          const std::shared_ptr<const RtpAudioFrameConfig>& config);
    void SetTrackSettings(int32_t number, const char* codec,
                          const std::shared_ptr<const RtpVideoFrameConfig>& config);
    std::shared_ptr<MemoryBuffer> TakeWrittenData();
private:
    static bool SetCodecSpecific(mkvmuxer::Track* track,
                                 const std::shared_ptr<const MemoryBuffer>& specific);
    mkvmuxer::Track* GetTrack(int32_t number) const;
    bool IsValidForTracksAdding() const;
    // 1kb buffer is enough for single OPUS frame
    // TODO: develop a strategy for optimal memory management for both audio & video (maybe mem pool)
    void ReserveBuffer() { Reserve(1024); }
    // impl. of mkvmuxer::IMkvWriter
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final;
    mkvmuxer::int64 Position() const final;
    mkvmuxer::int32 Position(mkvmuxer::int64 position) final;
    bool Seekable() const final { return true; }
    void ElementStartNotify(mkvmuxer::uint64, mkvmuxer::int64) final {}
private:
    mkvmuxer::Segment _segment;
    const bool _initialized;
    bool _wroteMedia = false;
    absl::flat_hash_map<int32_t, uint64_t> _tracksReference;
    size_t _audioTracksCount = 0UL;
    size_t _videoTracksCount = 0UL;
};

class RtpWebMSerializer::TrackInfo
{
public:
    TrackInfo(int32_t number, bool audio, uint32_t clockRate);
    int32_t GetNumber() const { return _number; }
    uint32_t GetClockRate() const { return _clockRate; }
    bool IsAudio() const { return _audio; }
    bool IsAccepted(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) const;
    // return true if changed
    bool SetCodec(RtpCodecMimeType::Subtype codec);
    bool SetCodec(const RtpCodecMimeType& mime) { return SetCodec(mime.GetSubtype()); }
    RtpCodecMimeType::Subtype GetCodec() const { return _codec; }
    void SetLastRtpTimeStamp(uint32_t lastRtpTimestamp);
    void SetLastRtpTimeStamp(const std::shared_ptr<const RtpMediaFrame>& mediaFrame);
    void ResetRtpTiming();
    uint64_t GetTimeStampNano() const;
    void SetLatestConfig(const std::shared_ptr<const RtpAudioFrameConfig>& config);
    void SetLatestConfig(const std::shared_ptr<const RtpVideoFrameConfig>& config);
    const std::shared_ptr<const RtpAudioFrameConfig>& GetLatestAudioConfig() const;
    const std::shared_ptr<const RtpVideoFrameConfig>& GetLatestVideoConfig() const;
private:
    template<bool forAudio> void TestConsistency() const;
private:
    const int32_t _number;
    const bool _audio;
    const uint32_t _clockRate;
    RtpCodecMimeType::Subtype _codec;
    uint32_t _lastRtpTimestamp = 0ULL;
    uint64_t _granule = 0ULL;
    std::shared_ptr<const RtpAudioFrameConfig> _latestAudioConfig;
    std::shared_ptr<const RtpVideoFrameConfig> _latestVideoConfig;
};

RtpWebMSerializer::RtpWebMSerializer()
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
                                 const std::shared_ptr<const RtpAudioFrameConfig>& config)
{
    if (ssrc) {
        const RtpCodecMimeType mime(RtpCodecMimeType::Type::AUDIO, codec);
        if(RtpCodecMimeType::Subtype::UNSET == codec || mime.IsAudioCodec()) {
            return AddMedia(ssrc, clockRate, mime, config);
        }
    }
    return false;
}

bool RtpWebMSerializer::AddVideo(uint32_t ssrc, uint32_t clockRate,
                                 RtpCodecMimeType::Subtype codec,
                                 const std::shared_ptr<const RtpVideoFrameConfig>& config)
{
    if (ssrc) {
        const RtpCodecMimeType mime(RtpCodecMimeType::Type::VIDEO, codec);
        if(RtpCodecMimeType::Subtype::UNSET == codec || mime.IsVideoCodec()) {
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
                const auto hasWroteMedia = _writer->HasWroteMedia();
                DestroyWriter(false);
                if (!_tracksInfo.empty()) {
                    InitWriter(hasWroteMedia);
                }
            }
        }
    }
}

void RtpWebMSerializer::Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    if (mediaFrame && _writer && HasDevices()) {
        if (const auto trackInfo = GetTrackInfo(mediaFrame)) {
            const auto timestamp = trackInfo->GetTimeStampNano();
            BeginWriteMediaPayload(mediaFrame->GetSsrc(), mediaFrame->IsKeyFrame(),
                                   mediaFrame->GetCodecMimeType(), mediaFrame->GetSequenceNumber(),
                                   mediaFrame->GetTimestamp(), mediaFrame->GetAbsSendtime());
            const auto ok = _writer->AddFrame(mediaFrame->GetPayload(),
                                              trackInfo->GetNumber(),
                                              timestamp,
                                              mediaFrame->IsKeyFrame());
            trackInfo->SetLastRtpTimeStamp(mediaFrame);
            WritePayload(_writer->TakeWrittenData());
            EndWriteMediaPayload(mediaFrame->GetSsrc(), ok);
            if (!ok) {
                const auto frameInfo = GetMediaFrameInfoString(mediaFrame);
                MS_ERROR("unable write frame to MKV media [%s] to track #%d",
                         frameInfo.c_str(), trackInfo->GetNumber());
                DestroyWriter(true);
            }
        }
    }
}

bool RtpWebMSerializer::IsCompatible(const RtpCodecMimeType& mimeType) const
{
    return IsSupported(mimeType);
}

RtpWebMSerializer::TrackInfo* RtpWebMSerializer::
    GetTrackInfo(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) const
{
    if (_writer && mediaFrame && mediaFrame->GetPayload()) {
        const auto& mime = mediaFrame->GetCodecMimeType();
        if (IsSupported(mime)) {
            const auto it = _tracksInfo.find(mediaFrame->GetSsrc());
            if (it != _tracksInfo.end() && it->second->IsAccepted(mediaFrame)) {
                const auto number = it->second->GetNumber();
                it->second->SetCodec(mime);
                const auto codec = GetCodecId(mime);
                if (it->second->IsAudio()) {
                    _writer->SetTrackSettings(number, codec, mediaFrame->GetAudioConfig());
                    it->second->SetLatestConfig(mediaFrame->GetAudioConfig());
                }
                else if (mediaFrame->IsKeyFrame()) { // video
                    _writer->SetTrackSettings(number, codec, mediaFrame->GetVideoConfig());
                    it->second->SetLatestConfig(mediaFrame->GetVideoConfig());
                }
                return it->second.get();
            }
        }
    }
    return nullptr;
}

void RtpWebMSerializer::InitWriter(bool restart)
{
    auto writer = std::make_unique<BufferedWriter>();
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
                const auto codec = GetCodecId(it->second->GetCodec());
                if (it->second->IsAudio()) {
                    writer->SetAudioSampleRate(number, it->second->GetClockRate(),
                                               IsOpusAudioCodec(it->second->GetCodec()));
                    writer->SetTrackSettings(number, codec, it->second->GetLatestAudioConfig());
                }
                else {
                    writer->SetTrackSettings(number, codec, it->second->GetLatestVideoConfig());
                }
            }
            else {
                break;
            }
        }
        if (ok) {
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
    if (ok) {
        StartStream(restart);
    }
    else { // failure
        EndStream(true);
    }
}

void RtpWebMSerializer::DestroyWriter(bool failure)
{
    if (_writer) {
        _writer->Finalize();
        if (HasDevices()) {
            WritePayload(_writer->TakeWrittenData());
        }
        _writer.reset();
        for (auto it = _tracksInfo.begin(); it != _tracksInfo.end(); ++it) {
            it->second->ResetRtpTiming();
        }
        EndStream(failure);
    }
}

template<class TConfig>
bool RtpWebMSerializer::AddMedia(uint32_t ssrc, uint32_t clockRate,
                                 const RtpCodecMimeType& mime,
                                 const std::shared_ptr<TConfig>& config)
{
    bool registered = false;
    if (ssrc && RtpCodecMimeType::Type::UNSET != mime.GetType()) {
        const auto it = _tracksInfo.find(ssrc);
        if (it == _tracksInfo.end()) {
            if (!_writer) {
                InitWriter(false);
            }
            else if(_writer->HasWroteMedia()) {
                DestroyWriter(false);
                InitWriter(true);
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
                        _writer->SetAudioSampleRate(number, clockRate, IsOpusAudioCodec(mime));
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

RtpWebMSerializer::BufferedWriter::BufferedWriter()
    : _initialized(_segment.Init(this))
{
    if (IsInitialized()) {
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

bool RtpWebMSerializer::BufferedWriter::AddFrame(const std::shared_ptr<const MemoryBuffer>& data,
                                                 int32_t number,
                                                 uint64_t timeCode, bool isKeyFrame)
{
    if (data && IsInitialized()) {
        const auto it = _tracksReference.find(number);
        if (it != _tracksReference.end()) {
            return _segment.AddFrame(data->GetData(), data->GetSize(), it->second,
                                     timeCode, isKeyFrame);
        }
    }
    return false;
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
                                                         const std::shared_ptr<const RtpAudioFrameConfig>& config)
{
    if (const auto track = static_cast<mkvmuxer::AudioTrack*>(GetTrack(number))) {
        track->set_codec_id(codec);
        if (config) {
            track->set_channels(config->GetChannelCount());
            track->set_bit_depth(config->GetBitsPerSample());
            if (!SetCodecSpecific(track, config->GetCodecSpecificData())) {
                MS_ERROR("failed to setup of MKV writer audio codec data");
            }
        }
    }
}

void RtpWebMSerializer::BufferedWriter::SetTrackSettings(int32_t number,
                                                         const char* codec,
                                                         const std::shared_ptr<const RtpVideoFrameConfig>& config)
{
    if (const auto track = static_cast<mkvmuxer::VideoTrack*>(GetTrack(number))) {
        track->set_codec_id(codec);
        if (config) {
            track->set_frame_rate(config->GetFrameRate());
            track->set_width(config->GetWidth());
            track->set_height(config->GetHeight());
            if (!SetCodecSpecific(track, config->GetCodecSpecificData())) {
                MS_ERROR("failed to setup of MKV writer video codec data");
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

mkvmuxer::int32 RtpWebMSerializer::BufferedWriter::Position(mkvmuxer::int64 position)
{
    Resize(position);
    return 0;
}

RtpWebMSerializer::TrackInfo::TrackInfo(int32_t number, bool audio, uint32_t clockRate)
    : _number(number)
    , _audio(audio)
    , _clockRate(clockRate)
    , _codec(RtpCodecMimeType::Subtype::UNSET)
{
    MS_ASSERT(_clockRate, "sample rate must be greater than zero");
}

bool RtpWebMSerializer::TrackInfo::IsAccepted(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) const
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

uint64_t RtpWebMSerializer::TrackInfo::GetTimeStampNano() const
{
    return MilliToNano(_granule) / GetClockRate();
}

void RtpWebMSerializer::TrackInfo::SetLastRtpTimeStamp(uint32_t lastRtpTimestamp)
{
    if (lastRtpTimestamp > _lastRtpTimestamp) {
        if (_lastRtpTimestamp) {
            _granule += lastRtpTimestamp - _lastRtpTimestamp;
        }
        _lastRtpTimestamp = lastRtpTimestamp;
    }
}

void RtpWebMSerializer::TrackInfo::SetLastRtpTimeStamp(const std::shared_ptr<const RtpMediaFrame>& mediaFrame)
{
    if (mediaFrame) {
        SetLastRtpTimeStamp(mediaFrame->GetTimestamp());
    }
}

void RtpWebMSerializer::TrackInfo::ResetRtpTiming()
{
    _lastRtpTimestamp = 0U;
    _granule = 0ULL;
}

void RtpWebMSerializer::TrackInfo::SetLatestConfig(const std::shared_ptr<const RtpAudioFrameConfig>& config)
{
    MS_ASSERT(IsAudio(), "set incorrect config for audio track");
    _latestAudioConfig = config;
}

void RtpWebMSerializer::TrackInfo::SetLatestConfig(const std::shared_ptr<const RtpVideoFrameConfig>& config)
{
    MS_ASSERT(!IsAudio(), "set incorrect config for video track");
    _latestVideoConfig = config;
}

const std::shared_ptr<const RtpAudioFrameConfig>& RtpWebMSerializer::TrackInfo::GetLatestAudioConfig() const
{
    MS_ASSERT(IsAudio(), "get incorrect config for audio track");
    return _latestAudioConfig;
}

const std::shared_ptr<const RtpVideoFrameConfig>& RtpWebMSerializer::TrackInfo::GetLatestVideoConfig() const
{
    MS_ASSERT(!IsAudio(), "get incorrect config for video track");
    return _latestVideoConfig;
}

} // namespace RTC
