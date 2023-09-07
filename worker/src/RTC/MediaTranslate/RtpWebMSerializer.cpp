#define MS_CLASS "RTC::RtpWebMSerializer"
#include "RTC/MediaTranslate/RtpWebMSerializer.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

namespace {

inline uint64_t MilliToNano(uint64_t milli) {
    return milli * 1000ULL * 1000ULL * 1000ULL;
}

inline bool IsOpusAudioCodec(RTC::RtpCodecMimeType::Subtype codec) {
    switch (codec) {
        case RTC::RtpCodecMimeType::Subtype::OPUS:
        case RTC::RtpCodecMimeType::Subtype::MULTIOPUS:
            // https://en.wikipedia.org/wiki/SILK
        case RTC::RtpCodecMimeType::Subtype::SILK: // needs to be tested
            return true;
        default:
            break;
    }
    return false;
}

inline bool IsOpusAudioCodec(const RTC::RtpCodecMimeType& mime) {
    return IsOpusAudioCodec(mime.GetSubtype());
}

inline static const char* GetCodecId(RTC::RtpCodecMimeType::Subtype codec) {
    switch (codec) {
        case RTC::RtpCodecMimeType::Subtype::VP8:
            return mkvmuxer::Tracks::kVp8CodecId;
        case RTC::RtpCodecMimeType::Subtype::VP9:
            return mkvmuxer::Tracks::kVp9CodecId;
        default:
            if (IsOpusAudioCodec(codec)) {
                return mkvmuxer::Tracks::kOpusCodecId;
            }
            break;
    }
    return nullptr;
}

inline static const char* GetCodecId(const RTC::RtpCodecMimeType& mime) {
    return GetCodecId(mime.GetSubtype());
}

}

namespace RTC
{

class RtpWebMSerializer::BufferedWriter : public mkvmuxer::IMkvWriter,
                                          private SimpleMemoryBuffer
{
public:
    BufferedWriter() { ReserveBuffer(); }
    std::shared_ptr<MemoryBuffer> TakeWrittenData();
    // impl. of mkvmuxer::IMkvWriter
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final;
    mkvmuxer::int64 Position() const final;
    mkvmuxer::int32 Position(mkvmuxer::int64 position) final;
    bool Seekable() const final { return true; }
    void ElementStartNotify(mkvmuxer::uint64, mkvmuxer::int64) final {}
private:
    // 1kb buffer is enough for single OPUS frame
    // TODO: develop a strategy for optimal memory management for both audio & video (maybe mem pool)
    void ReserveBuffer() { Reserve(1024); }
};

class RtpWebMSerializer::TrackInfo
{
public:
    TrackInfo(mkvmuxer::Track* track);
    uint64_t GetNumber() const { return _track->number(); }
    bool IsAudio() const { return mkvmuxer::Tracks::kAudio == _track->type(); }
    bool IsAccepted(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) const;
    void SetSeekPreRoll(uint64_t seekPreRoll) { _track->set_seek_pre_roll(seekPreRoll); }
    // return true if changed
    bool SetCodec(RtpCodecMimeType::Subtype codec);
    bool SetCodec(const RtpCodecMimeType& mime) { return SetCodec(mime.GetSubtype()); }
    RtpCodecMimeType::Subtype GetCodec() const { return _codec; }
    void SetLastRtpTimeStamp(uint32_t lastRtpTimestamp);
    void SetLastRtpTimeStamp(const std::shared_ptr<const RtpMediaFrame>& mediaFrame);
    uint64_t GetTimeStampNano(uint32_t sampleRate) const;
    void SetConfig(const RtpAudioFrameConfig* config);
    void SetConfig(const RtpVideoFrameConfig* config);
private:
    bool SetCodecPrivate(const MemoryBuffer* codecPrivate);
private:
    mkvmuxer::Track* const _track;
    RtpCodecMimeType::Subtype _codec;
    uint32_t _lastRtpTimestamp = 0ULL;
    uint64_t _granule = 0ULL;
};

RtpWebMSerializer::RtpWebMSerializer()
    : _writer(std::make_unique<BufferedWriter>())
{
    RtpWebMSerializer::SetLiveMode(true);
}

RtpWebMSerializer::~RtpWebMSerializer()
{
    RtpWebMSerializer::SetOutputDevice(nullptr);
}

bool RtpWebMSerializer::IsSupported(const RtpCodecMimeType& mimeType)
{
    return nullptr != GetCodecId(mimeType);
}

void RtpWebMSerializer::SetOutputDevice(OutputDevice* outputDevice)
{
    if (!outputDevice && GetOutputDevice()) {
        _segment.Finalize();
        CommitData(GetOutputDevice());
    }
    RtpMediaFrameSerializer::SetOutputDevice(outputDevice);
    if (outputDevice && !_segment.Init(_writer.get())) {
        MS_ERROR("failed to init of MKV writer segment");
    }
}

void RtpWebMSerializer::SetLiveMode(bool liveMode)
{
    RtpMediaFrameSerializer::SetLiveMode(liveMode);
    const auto mkvMode = liveMode ? mkvmuxer::Segment::kLive : mkvmuxer::Segment::kFile;
    const auto fileMode = mkvmuxer::Segment::kFile == mkvMode;
    _segment.set_mode(mkvMode);
    _segment.set_estimate_file_duration(fileMode);
    _segment.OutputCues(fileMode);
}

std::string_view RtpWebMSerializer::GetFileExtension(const RtpCodecMimeType&) const
{
    return "webm";
}

bool RtpWebMSerializer::RegisterAudio(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                                      const RtpAudioFrameConfig* config)
{
    if (ssrc) {
        if (RtpCodecMimeType::Subtype::UNSET != codec) {
            const RtpCodecMimeType mime(RtpCodecMimeType::Type::AUDIO, codec);
            MS_ASSERT(mime.IsAudioCodec(), "invalid audio codec");
        }
        return RegisterMedia<mkvmuxer::AudioTrack>(ssrc, codec, config);
    }
    return false;
}

bool RtpWebMSerializer::RegisterVideo(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                                      const RtpVideoFrameConfig* config)
{
    if (ssrc) {
        if (RtpCodecMimeType::Subtype::UNSET != codec) {
            const RtpCodecMimeType mime(RtpCodecMimeType::Type::VIDEO, codec);
            MS_ASSERT(mime.IsVideoCodec(), "invalid video codec");
        }
        return RegisterMedia<mkvmuxer::VideoTrack>(ssrc, codec, config);
    }
    return false;
}

void RtpWebMSerializer::Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    if (mediaFrame && mediaFrame->GetPayload()) {
        if (const auto outputDevice = GetOutputDevice()) {
            const auto trackInfo = GetTrackInfo(mediaFrame);
            if (trackInfo && trackInfo->IsAccepted(mediaFrame)) {
                const auto timestamp = trackInfo->GetTimeStampNano(mediaFrame->GetSampleRate());
                const auto& payload = mediaFrame->GetPayload();
                outputDevice->BeginWriteMediaPayload(mediaFrame->GetSsrc(),
                                                     mediaFrame->IsKeyFrame(),
                                                     mediaFrame->GetCodecMimeType(),
                                                     mediaFrame->GetSequenceNumber(),
                                                     mediaFrame->GetTimestamp(),
                                                     mediaFrame->GetAbsSendtime());
                const auto ok = _segment.AddFrame(payload->GetData(), payload->GetSize(),
                                                  trackInfo->GetNumber(), timestamp,
                                                  mediaFrame->IsKeyFrame());
                trackInfo->SetLastRtpTimeStamp(mediaFrame);
                CommitData(outputDevice);
                outputDevice->EndWriteMediaPayload(mediaFrame->GetSsrc(), ok);
                if (!ok) {
                    const auto frameInfo = GetMediaFrameInfoString(mediaFrame);
                    MS_ERROR("unable write frame to MKV media [%s] to track #%llu",
                             frameInfo.c_str(), trackInfo->GetNumber());
                }
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
    if (mediaFrame) {
        const auto& mime = mediaFrame->GetCodecMimeType();
        if (IsSupported(mime)) {
            const auto it = _tracksInfo.find(mediaFrame->GetSsrc());
            if (it != _tracksInfo.end()) {
                it->second->SetCodec(mime);
                if (IsOpusAudioCodec(mime)) {
                    // https://wiki.xiph.org/MatroskaOpus
                    if (48000U == mediaFrame->GetSampleRate()) {
                        it->second->SetSeekPreRoll(80000000ULL);
                    }
                }
                if (it->second->IsAudio()) {
                    it->second->SetConfig(mediaFrame->GetAudioConfig());
                }
                else {
                    it->second->SetConfig(mediaFrame->GetVideoConfig());
                }
                return it->second.get();
            }
        }
    }
    return nullptr;
}

template<class TMkvMediaTrack, class TConfig>
bool RtpWebMSerializer::RegisterMedia(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                                      const TConfig* config)
{
    bool registered = false;
    if (ssrc) {
        const auto it = _tracksInfo.find(ssrc);
        if (it == _tracksInfo.end()) {
            if (const auto track = AddMediaTrack(ssrc, config)) {
                auto trackInfo = std::make_unique<TrackInfo>(track);
                trackInfo->SetCodec(codec);
                trackInfo->SetConfig(config);
                _tracksInfo[ssrc] = std::move(trackInfo);
                registered = true;
            }
            else {
                MS_ERROR("unable to create MKV writer track, [SSRC = %du]", ssrc);
            }
        }
        else {
            registered = true; // already registered
        }
    }
    return registered;
}

mkvmuxer::Track* RtpWebMSerializer::AddMediaTrack(uint32_t ssrc, const RtpAudioFrameConfig* config)
{
    if (ssrc) {
        int32_t sampleRate = 0, channels = 0;
        if (config) {
            channels = config->_channelCount;
        }
        const auto expected = static_cast<int32_t>(_tracksInfo.size());
        const auto actual = _segment.AddAudioTrack(sampleRate, channels, expected);
        if (actual) {
            return _segment.GetTrackByNumber(actual);
        }
    }
    return nullptr;
}

mkvmuxer::Track* RtpWebMSerializer::AddMediaTrack(uint32_t ssrc, const RtpVideoFrameConfig* config)
{
    if (ssrc) {
        int32_t width = 0, height = 0;
        if (config) {
            width = config->_width;
            height = config->_height;
        }
        const auto expected = static_cast<int32_t>(_tracksInfo.size());
        const auto actual = _segment.AddVideoTrack(width, height, expected);
        if (actual) {
            const auto videoTrack = static_cast<mkvmuxer::VideoTrack*>(_segment.GetTrackByNumber(actual));
            if (videoTrack) {
                videoTrack->SetAlphaMode(mkvmuxer::VideoTrack::kNoAlpha);
            }
            return videoTrack;
        }
    }
    return nullptr;
}

void RtpWebMSerializer::CommitData(OutputDevice* outputDevice)
{
    if (outputDevice) {
        if (const auto buffer = _writer->TakeWrittenData()) {
            outputDevice->Write(buffer);
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

mkvmuxer::int32 RtpWebMSerializer::BufferedWriter::Write(const void* buf, mkvmuxer::uint32 len)
{
    return Append(buf, len) ? 0 : 1;
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

RtpWebMSerializer::TrackInfo::TrackInfo(mkvmuxer::Track* track)
    : _track(track)
    , _codec(RtpCodecMimeType::Subtype::UNSET)
{
}

bool RtpWebMSerializer::TrackInfo::IsAccepted(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) const
{
    return mediaFrame && mediaFrame->GetTimestamp() > _lastRtpTimestamp;
}

bool RtpWebMSerializer::TrackInfo::SetCodec(RtpCodecMimeType::Subtype codec)
{
    if (codec != _codec) {
        if (const auto codecId = GetCodecId(codec)) {
            _codec = codec;
            _track->set_codec_id(codecId);
            return true;
        }
    }
    return false;
}

uint64_t RtpWebMSerializer::TrackInfo::GetTimeStampNano(uint32_t sampleRate) const
{
    MS_ASSERT(sampleRate, "invalid sample rate of media frame");
    return MilliToNano(_granule) / sampleRate;
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

void RtpWebMSerializer::TrackInfo::SetConfig(const RtpAudioFrameConfig* config)
{
    if (config) {
        MS_ASSERT(IsAudio(), "is not audio track");
        const auto audioTrack = static_cast<mkvmuxer::AudioTrack*>(_track);
        audioTrack->set_channels(config->_channelCount);
        audioTrack->set_bit_depth(config->_bitsPerSample);
        if (!SetCodecPrivate(config->_codecSpecificData.get())) {
            MS_ERROR("failed to setup of MKV writer audio codec data");
        }
    }
}

void RtpWebMSerializer::TrackInfo::SetConfig(const RtpVideoFrameConfig* config)
{
    if (config) {
        MS_ASSERT(!IsAudio(), "is not video track");
        const auto videoTrack = static_cast<mkvmuxer::VideoTrack*>(_track);
        videoTrack->set_frame_rate(config->_frameRate);
        videoTrack->set_width(config->_width);
        videoTrack->set_height(config->_height);
        if (!SetCodecPrivate(config->_codecSpecificData.get())) {
            MS_ERROR("failed to setup of MKV writer video codec data");
        }
    }
}

bool RtpWebMSerializer::TrackInfo::SetCodecPrivate(const MemoryBuffer* codecPrivate)
{
    return !codecPrivate || _track->SetCodecPrivate(codecPrivate->GetData(), codecPrivate->GetSize());
}

} // namespace RTC
