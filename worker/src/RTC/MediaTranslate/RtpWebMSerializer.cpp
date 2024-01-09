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
    TrackInfo(mkvmuxer::Track* track, bool audio, RtpCodecMimeType::Subtype codec);
    uint64_t GetNumber() const { return _track->number(); }
    mkvmuxer::Track* GetTrack() const { return _track; }
    bool IsAudio() const { return _audio; }
    bool IsAccepted(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) const;
    // return true if changed
    bool SetCodec(const std::shared_ptr<const RtpMediaFrame>& mediaFrame);
    RtpCodecMimeType::Subtype GetCodec() const { return _codec; }
    void SetLastRtpTimeStamp(uint32_t lastRtpTimestamp);
    void SetLastRtpTimeStamp(const std::shared_ptr<const RtpMediaFrame>& mediaFrame);
    uint64_t GetTimeStampNano(uint32_t sampleRate) const;
private:
    bool SetCodec(RtpCodecMimeType::Subtype codec, bool force = false);
    bool SetCodec(const RtpCodecMimeType& mime) { return SetCodec(mime.GetSubtype()); }
    bool SetCodecPrivate(const std::shared_ptr<const RtpMediaFrame>& mediaFrame);
private:
    mkvmuxer::Track* const _track;
    const bool _audio;
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

RtpWebMSerializer::TrackInfo* RtpWebMSerializer::GetTrackInfo(const std::shared_ptr<const RtpMediaFrame>& mediaFrame)
{
    TrackInfo* trackInfo = nullptr;
    if (mediaFrame && GetCodecId(mediaFrame->GetCodecMimeType())) {
        auto& sharedTrackInfo = mediaFrame->IsAudio() ? _audioTrackInfo : _videoTrackInfo;
        if (!sharedTrackInfo) {
            auto& hasError = mediaFrame->IsAudio() ? _hasAudioTrackCreationError : _hasVideoTrackCreationError;
            if (!hasError) {
                if (const auto track = CreateMediaTrack(mediaFrame)) {
                    sharedTrackInfo = std::make_unique<TrackInfo>(track, mediaFrame->IsAudio(),
                                                                  mediaFrame->GetCodecMimeType().GetSubtype());
                    /*if (_videoTrackInfo || (_audioTrackInfo && !_videoTrackInfo)) {
                        if (!_segment.CuesTrack(track->number())) {
                            const auto frameInfo = GetMediaFrameInfoString(mediaFrame);
                            MS_ERROR("failed to cue MKV writer media [%s] track #%llu",
                                     frameInfo.c_str(), track->number());
                        }
                    }*/
                }
                else {
                    hasError = true;
                    const auto frameInfo = GetMediaFrameInfoString(mediaFrame);
                    MS_ERROR("unable to create MKV writer [%s] track", frameInfo.c_str());
                }
            }
        }
        trackInfo = sharedTrackInfo.get();
        if (mkvmuxer::Track* track = trackInfo ? trackInfo->GetTrack() : nullptr) {
            trackInfo->SetCodec(mediaFrame);
            if (const auto config = mediaFrame->GetAudioConfig()) {
                if (const auto audioTrack = static_cast<mkvmuxer::AudioTrack*>(track)) {
                    audioTrack->set_bit_depth(config->_bitsPerSample);
                    audioTrack->set_channels(config->_channelCount);
                    audioTrack->set_sample_rate(mediaFrame->GetSampleRate());
                }
            }
            else if (const auto config = mediaFrame->GetVideoConfig()) {
                if (const auto videoTrack = static_cast<mkvmuxer::VideoTrack*>(track)) {
                    if (mediaFrame->IsKeyFrame()) {
                        videoTrack->SetAlphaMode(mkvmuxer::VideoTrack::kNoAlpha);
                        videoTrack->set_frame_rate(config->_frameRate);
                        videoTrack->set_width(config->_width);
                        videoTrack->set_height(config->_height);
                    }
                }
            }
        }
    }
    return trackInfo;
}

mkvmuxer::Track* RtpWebMSerializer::CreateMediaTrack(const std::shared_ptr<const RtpMediaFrame>& mediaFrame)
{
    if (mediaFrame) {
        uint64_t trackNumber = 0ULL;
        if (mediaFrame->IsAudio()) {
            if (const auto config = mediaFrame->GetAudioConfig()) {
                trackNumber = _segment.AddAudioTrack(mediaFrame->GetSampleRate(),
                                                     config->_channelCount,
                                                     _audioTrackId);
            }
        }
        else if (const auto config = mediaFrame->GetVideoConfig()) {
            trackNumber = _segment.AddVideoTrack(config->_width, config->_height, _videoTrackId);
        }
        if (trackNumber) {
            return _segment.GetTrackByNumber(trackNumber);
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

RtpWebMSerializer::TrackInfo::TrackInfo(mkvmuxer::Track* track, bool audio,
                                        RtpCodecMimeType::Subtype codec)
    : _track(track)
    , _audio(audio)
{
    SetCodec(codec, true);
}

bool RtpWebMSerializer::TrackInfo::IsAccepted(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) const
{
    return mediaFrame && mediaFrame->GetTimestamp() > _lastRtpTimestamp;
}

uint64_t RtpWebMSerializer::TrackInfo::GetTimeStampNano(uint32_t sampleRate) const
{
    MS_ASSERT(sampleRate, "invalid sample rate of media frame");
    return MilliToNano(_granule) / sampleRate;
}

bool RtpWebMSerializer::TrackInfo::SetCodec(const std::shared_ptr<const RtpMediaFrame>& mediaFrame)
{
    bool changed = false;
    if (mediaFrame) {
        changed = SetCodec(mediaFrame->GetCodecMimeType());
        if (changed) {
            const auto& mime = mediaFrame->GetCodecMimeType();
            if (IsOpusAudioCodec(mime)) {
                // https://wiki.xiph.org/MatroskaOpus
                if (48000U == mediaFrame->GetSampleRate()) {
                    _track->set_seek_pre_roll(80000000ULL);
                }
            }
        }
        if (!SetCodecPrivate(mediaFrame)) {
            const auto frameInfo = GetMediaFrameInfoString(mediaFrame);
            MS_ERROR("failed to setup of MKV writer codec data [%s]", frameInfo.c_str());
        }
    }
    return changed;
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

bool RtpWebMSerializer::TrackInfo::SetCodec(RtpCodecMimeType::Subtype codec, bool force)
{
    if (force || codec != _codec) {
        if (const auto codecId = GetCodecId(codec)) {
            _codec = codec;
            _track->set_codec_id(codecId);
            return true;
        }
    }
    return false;
}

bool RtpWebMSerializer::TrackInfo::SetCodecPrivate(const std::shared_ptr<const RtpMediaFrame>& mediaFrame)
{
    bool ok = false;
    if (mediaFrame) {
        const MemoryBuffer* data = nullptr;
        if (const auto config = mediaFrame->GetAudioConfig()) {
            data = config->_codecSpecificData.get();
        }
        else if (const auto config = mediaFrame->GetVideoConfig()) {
            data = config->_codecSpecificData.get();
        }
        if (data) {
            ok = _track->SetCodecPrivate(data->GetData(), data->GetSize());
        }
        else {
            ok = true;
        }
    }
    return ok;
}

} // namespace RTC
