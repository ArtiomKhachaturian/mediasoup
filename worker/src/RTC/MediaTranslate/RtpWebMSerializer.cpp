#define MS_CLASS "RTC::RtpWebMSerializer"
#include "RTC/MediaTranslate/RtpWebMSerializer.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "MemoryBuffer.h"
#include "Utils.hpp"
#include "Logger.hpp"

namespace RTC
{

class RtpWebMSerializer::BufferedWriter : public mkvmuxer::IMkvWriter
{
public:
    BufferedWriter() { ReserveBuffer(); }
    std::shared_ptr<MemoryBuffer> takeBuffer();
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

class RtpWebMSerializer::MediaBufferImpl : public MemoryBuffer
{
public:
    MediaBufferImpl(std::vector<uint8_t> buffer);
    // impl. of MemoryBuffer
    size_t GetSize() const final { return _buffer.size(); }
    uint8_t* GetData() { return _buffer.data(); }
    const uint8_t* GetData() const { return _buffer.data(); }
private:
    std::vector<uint8_t> _buffer;
};

struct RtpWebMSerializer::TrackInfo
{
    const uint64_t _number;
    const uint32_t _rate;
    uint64_t _granule = 0ULL;
    TrackInfo(uint64_t number, uint32_t rate);
    uint64_t GetTimestampNs() const;
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
    return nullptr != GetCodec(mimeType);
}

void RtpWebMSerializer::SetOutputDevice(OutputDevice* outputDevice)
{
    if (!outputDevice && GetOutputDevice()) {
        _segment.Finalize();
        CommitData(GetOutputDevice());
    }
    RtpMediaFrameSerializer::SetOutputDevice(outputDevice);
    if (outputDevice) {
        _segment.Init(_writer.get());
    }
}

void RtpWebMSerializer::SetLiveMode(bool liveMode)
{
    RtpMediaFrameSerializer::SetLiveMode(liveMode);
    _segment.set_mode(liveMode ?  mkvmuxer::Segment::kLive : mkvmuxer::Segment::kFile);
}

void RtpWebMSerializer::Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    if (mediaFrame) {
        if (const auto outputDevice = GetOutputDevice()) {
            if (const auto track = GetTrack(mediaFrame)) {
                const auto& payload = mediaFrame->GetPayload();
                outputDevice->BeginWriteMediaPayload(mediaFrame->GetSsrc(),
                                                     mediaFrame->IsKeyFrame(),
                                                     mediaFrame->GetCodecMimeType(),
                                                     mediaFrame->GetSequenceNumber(),
                                                     mediaFrame->GetTimestamp(),
                                                     mediaFrame->GetAbsSendtime(),
                                                     mediaFrame->GetDuration());
                const auto ok = _segment.AddFrame(payload.data(), payload.size(), track->_number,
                                                  track->GetTimestampNs(), mediaFrame->IsKeyFrame());
                if (ok) {
                    track->_granule += mediaFrame->GetDuration();
                    if (mkvmuxer::Segment::kLive == _segment.mode()) {
                        _segment.CuesTrack(track->_number); // for live mode
                    }
                }
                CommitData(outputDevice);
                outputDevice->EndWriteMediaPayload(mediaFrame->GetSsrc(), ok);
            }
        }
    }
}

bool RtpWebMSerializer::IsOpusAudio(const RtpCodecMimeType& mimeType)
{
    switch (mimeType.subtype) {
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

const char* RtpWebMSerializer::GetCodec(const RtpCodecMimeType& mimeType)
{
    switch (mimeType.subtype) {
        case RtpCodecMimeType::Subtype::VP8:
            return mkvmuxer::Tracks::kVp8CodecId;
        case RtpCodecMimeType::Subtype::VP9:
            return mkvmuxer::Tracks::kVp9CodecId;
        default:
            if (IsOpusAudio(mimeType)) {
                return mkvmuxer::Tracks::kOpusCodecId;
            }
            break;
    }
    return nullptr;
}

bool RtpWebMSerializer::SetupTrack(mkvmuxer::AudioTrack* track, const RtpAudioConfig* config,
                                   const RtpCodecMimeType& mimeType)
{
    if (track && config) {
        track->set_bit_depth(config->GetBitsPerSample());
        if (IsOpusAudio(mimeType)) {
            if (48000U == config->GetSampleRate()) { // https://wiki.xiph.org/MatroskaOpus
                track->set_seek_pre_roll(80000000ULL);
            }
            Codecs::Opus::OpusHead head(config->GetChannelCount(), config->GetSampleRate());
            track->SetCodecPrivate(reinterpret_cast<const uint8_t*>(&head), sizeof(head));
        }
        return true;
    }
    return false;
}

bool RtpWebMSerializer::SetupTrack(mkvmuxer::VideoTrack* track, const RtpVideoConfig* config,
                                   const RtpCodecMimeType& /*mimeType*/)
{
    if (track && config) {
        track->set_frame_rate(config->GetFrameRate());
        return true;
    }
    return false;
}

bool RtpWebMSerializer::SetupTrack(uint64_t trackNumber, const RtpAudioConfig* config,
                                   const RtpCodecMimeType& mimeType)
{
    if (trackNumber && config) {
        const auto track = static_cast<mkvmuxer::AudioTrack*>(_segment.GetTrackByNumber(trackNumber));
        return SetupTrack(track, config, mimeType);
    }
    return false;
}

bool RtpWebMSerializer::SetupTrack(uint64_t trackNumber, const RtpVideoConfig* config,
                                   const RtpCodecMimeType& mimeType)
{
    if (trackNumber && config) {
        const auto track = static_cast<mkvmuxer::VideoTrack*>(_segment.GetTrackByNumber(trackNumber));
        return SetupTrack(track, config, mimeType);
    }
    return false;
}

RtpWebMSerializer::TrackInfo* RtpWebMSerializer::GetTrack(const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    RtpWebMSerializer::TrackInfo* trackInfo = nullptr;
    if (mediaFrame) {
        size_t configKey = 0UL;
        if (const auto config = mediaFrame->GetAudioConfig()) {
            configKey = Utils::HashCombine(config->GetSampleRate(),
                                           config->GetChannelCount(),
                                           config->GetBitsPerSample());
        }
        else if (const auto config = mediaFrame->GetVideoConfig()) {
            configKey = Utils::HashCombine(config->GetWidth(),
                                           config->GetHeight(),
                                           config->GetFrameRate());
        }
        else {
            MS_ASSERT(false, "no media config provided");
        }
        if (configKey) {
            configKey = Utils::HashCombine(mediaFrame->GetCodecMimeType().subtype, configKey);
            auto it = _tracks.find(configKey);
            if (it == _tracks.end()) {
                const auto& mime = mediaFrame->GetCodecMimeType();
                if (const auto frameCodec = GetCodec(mime)) {
                    uint64_t trackNumber = 0ULL;
                    uint32_t rate = 0U;
                    if (const auto config = mediaFrame->GetAudioConfig()) {
                        trackNumber = _segment.AddAudioTrack(config->GetSampleRate(),
                                                             config->GetChannelCount(),
                                                             static_cast<mkvmuxer::int32>(_tracks.size()) + 1);
                        if (SetupTrack(trackNumber, config, mime)) {
                            rate = config->GetSampleRate();
                        }
                    }
                    else if (const auto config = mediaFrame->GetVideoConfig()) {
                        trackNumber = _segment.AddVideoTrack(config->GetWidth(),
                                                             config->GetHeight(),
                                                             static_cast<mkvmuxer::int32>(_tracks.size()) + 1);
                        if (SetupTrack(trackNumber, config, mime)) {
                            rate = static_cast<uint32_t>(std::round(config->GetFrameRate()));
                        }
                    }
                    if (trackNumber) {
                        _segment.GetTrackByNumber(trackNumber)->set_codec_id(frameCodec);
                        it = _tracks.emplace(configKey, TrackInfo(trackNumber, rate)).first;
                        trackInfo = &it->second;
                    }
                }
                else {
                    MS_ASSERT(false, "unsupported media codec");
                }
            }
            else {
                trackInfo = &it->second;
            }
        }
    }
    return trackInfo;
}

void RtpWebMSerializer::CommitData(OutputDevice* outputDevice)
{
    if (outputDevice) {
        if (const auto buffer = _writer->takeBuffer()) {
            outputDevice->Write(buffer);
        }
    }
}

std::shared_ptr<MemoryBuffer> RtpWebMSerializer::BufferedWriter::takeBuffer()
{
    if (!_buffer.empty()) {
        auto buffer = std::make_shared<MediaBufferImpl>(std::move(_buffer));
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

RtpWebMSerializer::MediaBufferImpl::MediaBufferImpl(std::vector<uint8_t> buffer)
    : _buffer(std::move(buffer))
{
}

RtpWebMSerializer::TrackInfo::TrackInfo(uint64_t number, uint32_t rate)
    : _number(number)
    , _rate(rate)
{
}

uint64_t RtpWebMSerializer::TrackInfo::GetTimestampNs() const
{
    return (_granule * 1000ULL * 1000ULL * 1000ULL) / _rate;
}

} // namespace RTC
