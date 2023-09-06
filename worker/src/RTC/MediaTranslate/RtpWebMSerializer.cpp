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

}

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

class RtpWebMSerializer::TrackInfo
{
public:
    TrackInfo(uint64_t number, RtpCodecMimeType::Subtype codec = RtpCodecMimeType::Subtype::UNSET);
    uint64_t GetNumber() const { return _number; }
    // return true if changed
    bool SetCodec(RtpCodecMimeType::Subtype codec);
    RtpCodecMimeType::Subtype GetCodec() const { return _codec; }
    void AddWrittenSamples(uint32_t samplesCount) { _granule += samplesCount; }
    uint64_t GetTimeStampNano(uint32_t sampleRate) const;
private:
    const uint64_t _number;
    RtpCodecMimeType::Subtype _codec;
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
    return nullptr != GetCodec(mimeType);
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
    _segment.set_mode(liveMode ?  mkvmuxer::Segment::kLive : mkvmuxer::Segment::kFile);
}

std::string_view RtpWebMSerializer::GetFileExtension(const RtpCodecMimeType&) const
{
    return "webm";
}

void RtpWebMSerializer::Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    if (mediaFrame && mediaFrame->GetPayload()) {
        if (const auto outputDevice = GetOutputDevice()) {
            if (const auto track = GetTrack(mediaFrame)) {
                const auto timestamp = track->GetTimeStampNano(mediaFrame->GetSampleRate());
                const auto& payload = mediaFrame->GetPayload();
                outputDevice->BeginWriteMediaPayload(mediaFrame->GetSsrc(),
                                                     mediaFrame->IsKeyFrame(),
                                                     mediaFrame->GetCodecMimeType(),
                                                     mediaFrame->GetSequenceNumber(),
                                                     mediaFrame->GetTimestamp(),
                                                     mediaFrame->GetAbsSendtime());
                const auto ok = _segment.AddFrame(payload->GetData(), payload->GetSize(),
                                                  track->GetNumber(), timestamp,
                                                  mediaFrame->IsKeyFrame());
                if (ok) {
                    track->AddWrittenSamples(mediaFrame->GetSamplesCount());
                }
                else {
                    // TODO: log error ?
                }
                CommitData(outputDevice);
                outputDevice->EndWriteMediaPayload(mediaFrame->GetSsrc(), ok);
            }
        }
    }
}

bool RtpWebMSerializer::IsOpusAudio(const RtpCodecMimeType& mimeType)
{
    switch (mimeType.GetSubtype()) {
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
    switch (mimeType.GetSubtype()) {
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

RtpWebMSerializer::TrackInfo* RtpWebMSerializer::GetTrack(const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    RtpWebMSerializer::TrackInfo* trackInfo = nullptr;
    if (mediaFrame) {
        const auto& mimeType = mediaFrame->GetCodecMimeType();
        if (const auto codecId = GetCodec(mimeType)) {
            const auto ssrc = mediaFrame->GetSsrc();
            auto it = _tracks.find(ssrc);
            if (it == _tracks.end()) {
                if (codecId) {
                    const mkvmuxer::int32 trackNumber = static_cast<mkvmuxer::int32>(_tracks.size()) + 1;
                    bool added = false;
                    if (const auto config = mediaFrame->GetAudioConfig()) {
                        added = 0 != _segment.AddAudioTrack(mediaFrame->GetSampleRate(),
                                                            config->_channelCount,
                                                            trackNumber);
                    }
                    else if (const auto config = mediaFrame->GetVideoConfig()) {
                        added = 0 != _segment.AddVideoTrack(config->_width, config->_height,
                                                            trackNumber);
                    }
                    if (added) {
                        it = _tracks.emplace(ssrc, TrackInfo(trackNumber)).first;
                        trackInfo = &it->second;
                    }
                    else {
                        const auto errorInfo = GetStreamInfoString(mimeType, mediaFrame->GetSsrc());
                        MS_ERROR("failed add MKV writer media track for %s", errorInfo.c_str());
                    }
                }
            }
            else {
                trackInfo = &it->second;
            }
            if (trackInfo) {
                if (const auto track = _segment.GetTrackByNumber(trackInfo->GetNumber())) {
                    const auto codecTypeChanged = trackInfo->SetCodec(mimeType.GetSubtype());
                    if (const auto config = mediaFrame->GetAudioConfig()) {
                        if (const auto audioTrack = static_cast<mkvmuxer::AudioTrack*>(track)) {
                            audioTrack->set_bit_depth(config->_bitsPerSample);
                            audioTrack->set_channels(config->_channelCount);
                            audioTrack->set_sample_rate(mediaFrame->GetSampleRate());
                            if (codecTypeChanged && IsOpusAudio(mediaFrame->GetCodecMimeType())) {
                                // https://wiki.xiph.org/MatroskaOpus
                                if (48000U == mediaFrame->GetSampleRate()) {
                                    track->set_seek_pre_roll(80000000ULL);
                                }
                                Codecs::Opus::OpusHead head(config->_channelCount, mediaFrame->GetSampleRate());
                                audioTrack->SetCodecPrivate(reinterpret_cast<const uint8_t*>(&head), sizeof(head));
                            }
                        }
                    }
                    else if (const auto config = mediaFrame->GetVideoConfig()) {
                        if (const auto videoTrack = static_cast<mkvmuxer::VideoTrack*>(track)) {
                            if (mediaFrame->IsKeyFrame()) {
                                videoTrack->set_frame_rate(config->_frameRate);
                                videoTrack->set_width(config->_width);
                                videoTrack->set_height(config->_height);
                            }
                        }
                    }
                    if (codecTypeChanged) {
                        track->set_codec_id(codecId);
                    }
                }
                else {
                    const auto errorInfo = GetStreamInfoString(mimeType, mediaFrame->GetSsrc());
                    MS_ERROR("unable to find MKV writer media track #%llu for %s",
                             trackInfo->GetNumber(), errorInfo.c_str());
                }
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

RtpWebMSerializer::TrackInfo::TrackInfo(uint64_t number, RtpCodecMimeType::Subtype codec)
    : _number(number)
    , _codec(codec)
{
}

bool RtpWebMSerializer::TrackInfo::SetCodec(RtpCodecMimeType::Subtype codec)
{
    if (codec != _codec) {
        _codec = codec;
        return true;
    }
    return false;
}

uint64_t RtpWebMSerializer::TrackInfo::GetTimeStampNano(uint32_t sampleRate) const
{
    MS_ASSERT(sampleRate, "invalid sample rate of media frame");
    return MilliToNano(_granule) / sampleRate;
}

} // namespace RTC
