#define MS_CLASS "RTC::RtpWebMSerializer"
#include "RTC/MediaTranslate/RtpWebMSerializer.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/Codecs/Opus.hpp"
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

class RtpWebMSerializer::BufferedWriter : public mkvmuxer::IMkvWriter
{
public:
    BufferedWriter() { ReserveBuffer(); }
    std::shared_ptr<MemoryBuffer> TakeBuffer();
    // impl. of mkvmuxer::IMkvWriter
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final;
    mkvmuxer::int64 Position() const final;
    mkvmuxer::int32 Position(mkvmuxer::int64 position) final;
    bool Seekable() const final { return true; }
    void ElementStartNotify(mkvmuxer::uint64, mkvmuxer::int64) final {}
private:
    // 1kb buffer is enough for single OPUS frame
    // TODO: develop a strategy for optimal memory management for both audio & video (maybe mem pool)
    void ReserveBuffer() { _buffer.reserve(1024); }
private:
    std::vector<uint8_t> _buffer;
};

class RtpWebMSerializer::TrackInfo
{
public:
    TrackInfo(mkvmuxer::Track* track, bool audio, RtpCodecMimeType::Subtype codec = RtpCodecMimeType::Subtype::UNSET);
    uint64_t GetNumber() const { return _track->number(); }
    mkvmuxer::Track* GetTrack() const { return _track; }
    bool IsAudio() const { return _audio; }
    bool IsAccepted(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) const;
    // return true if changed
    bool SetCodec(RtpCodecMimeType::Subtype codec);
    bool SetCodec(const RtpCodecMimeType& mime) { return SetCodec(mime.GetSubtype()); }
    bool SetCodec(const std::shared_ptr<const RtpMediaFrame>& mediaFrame);
    RtpCodecMimeType::Subtype GetCodec() const { return _codec; }
    void SetLastRtpTimeStamp(uint32_t lastRtpTimestamp);
    uint64_t GetTimeStampNano(uint32_t sampleRate) const;
private:
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
    if (liveMode) {
        _segment.set_mode(mkvmuxer::Segment::kLive);
        _segment.set_estimate_file_duration(false);
    }
    else {
        _segment.set_mode(mkvmuxer::Segment::kFile);
        _segment.set_estimate_file_duration(true);
    }
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
                trackInfo->SetLastRtpTimeStamp(mediaFrame->GetTimestamp());
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
        const auto it = _tracksInfo.find(mediaFrame->GetSsrc());
        if (it == _tracksInfo.end()) {
            auto& trackNumber = mediaFrame->IsAudio() ? _audioTrackNumber : _videoTrackNumber;
            auto track = _segment.GetTrackByNumber(trackNumber);
            if (!track) {
                track = CreateMediaTrack(mediaFrame);
                if (track) {
                    trackNumber = track->number();
                    if (!mediaFrame->IsAudio() || 0ULL == _videoTrackNumber) {
                        if (!_segment.CuesTrack(trackNumber)) {
                            const auto frameInfo = GetMediaFrameInfoString(mediaFrame);
                            MS_ERROR("failed to cue MKV writer media [%s] track #%llu",
                                     frameInfo.c_str(), trackNumber);
                        }
                    }
                }
            }
            if (track) {
                auto addedTrackInfo = std::make_unique<TrackInfo>(track, mediaFrame->IsAudio());
                trackInfo = addedTrackInfo.get();
                _tracksInfo[mediaFrame->GetSsrc()] = std::move(addedTrackInfo);
            }
            else {
                const auto frameInfo = GetMediaFrameInfoString(mediaFrame);
                MS_ERROR("failed to obtain MKV writer [%s] track", frameInfo.c_str());
                _tracksInfo[mediaFrame->GetSsrc()] = nullptr;
            }
        }
        else {
            trackInfo = it->second.get();
        }
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
        if (const auto buffer = _writer->TakeBuffer()) {
            outputDevice->Write(buffer);
        }
    }
}

std::shared_ptr<MemoryBuffer> RtpWebMSerializer::BufferedWriter::TakeBuffer()
{
    if (!_buffer.empty()) {
        auto buffer = std::make_shared<SimpleMemoryBuffer>(std::move(_buffer));
        ReserveBuffer();
        return buffer;
    }
    return nullptr;
}

mkvmuxer::int32 RtpWebMSerializer::BufferedWriter::Write(const void* buf,
                                                         mkvmuxer::uint32 len)
{
    if (buf && len) {
        auto newBytes = reinterpret_cast<const uint8_t*>(buf);
        std::copy(newBytes, newBytes + len, std::back_inserter(_buffer));
        return 0;
    }
    return 1;
}

mkvmuxer::int64 RtpWebMSerializer::BufferedWriter::Position() const
{
    return static_cast<mkvmuxer::int64>(_buffer.size());
}

mkvmuxer::int32 RtpWebMSerializer::BufferedWriter::Position(mkvmuxer::int64 position)
{
    _buffer.resize(position);
    return 0;
}

RtpWebMSerializer::TrackInfo::TrackInfo(mkvmuxer::Track* track, bool audio,
                                        RtpCodecMimeType::Subtype codec)
    : _track(track)
    , _audio(audio)
    , _codec(RtpCodecMimeType::Subtype::UNSET)
{
    SetCodec(codec);
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

bool RtpWebMSerializer::TrackInfo::SetCodec(const std::shared_ptr<const RtpMediaFrame>& mediaFrame)
{
    const auto changed = mediaFrame && SetCodec(mediaFrame->GetCodecMimeType());
    if (changed) {
        const auto& mime = mediaFrame->GetCodecMimeType();
        if (IsOpusAudioCodec(mime)) {
            // https://wiki.xiph.org/MatroskaOpus
            if (48000U == mediaFrame->GetSampleRate()) {
                _track->set_seek_pre_roll(80000000ULL);
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

bool RtpWebMSerializer::TrackInfo::SetCodecPrivate(const std::shared_ptr<const RtpMediaFrame>& mediaFrame)
{
    bool ok = false;
    if (mediaFrame) {
        const auto& mime = mediaFrame->GetCodecMimeType();
        if (IsOpusAudioCodec(mime)) {
            if (const auto config = mediaFrame->GetAudioConfig()) {
                Codecs::Opus::OpusHead head(config->_channelCount, mediaFrame->GetSampleRate());
                ok = _track->SetCodecPrivate(reinterpret_cast<const uint8_t*>(&head), sizeof(head));
            }
        }
        else {
            ok = true;
        }
    }
    return ok;
}

} // namespace RTC
