#define MS_CLASS "RTC::RtpAudioWebMSerializer"
#include "RTC/Codecs/RtpAudioWebMSerializer.hpp"
#include "RTC/OutputDevice.hpp"
#include "RTC/RtpMediaFrame.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

namespace RTC
{

#pragma pack(push)
#pragma pack(1)
struct RtpAudioWebMSerializer::OpusMkvCodecPrivate
{
    const uint8_t _head[8] = {0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64};
    const uint8_t _version = 1U;
    uint8_t _channelCount = _defaultChannelCount;
    const uint16_t _preSkip = 0U;
    uint32_t _sampleRate = _defaultSampleRate;
    const uint16_t _gain = 0U;
    const uint8_t _mappingFamily = 0U;
};
#pragma pack(pop)

struct RtpAudioWebMSerializer::TrackInfo
{
    uint64_t _number;
    uint32_t _sampleRate;
    uint64_t _granule = 0ULL;
    TrackInfo(uint64_t number, uint32_t sampleRate);
    uint64_t GetTimestampNs() const;
};

RtpAudioWebMSerializer::RtpAudioWebMSerializer(OutputDevice* outputDevice, bool opusCodec)
    : RtpMediaFrameSerializer(outputDevice)
    , _opusCodec(opusCodec)
{
    _segment.Init(this);
    _segment.set_mode(outputDevice->IsFileDevice() ? mkvmuxer::Segment::kFile : mkvmuxer::Segment::kLive);
}

RtpAudioWebMSerializer::~RtpAudioWebMSerializer()
{
    _segment.Finalize();
}

void RtpAudioWebMSerializer::Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    if (mediaFrame) {
        const auto& mimeType = mediaFrame->GetCodecMimeType();
        MS_ASSERT(RtpCodecMimeType::Type::AUDIO == mimeType.type, "incorrect frame media type");
        if (const auto track = GetTrack(mediaFrame->GetAudioConfig())) {
            const auto& payload = mediaFrame->GetPayload();
            GetOutputDevice()->BeginWriteMediaPayload(mediaFrame->GetSsrc(),
                                                      mediaFrame->IsKeyFrame(), mimeType,
                                                      mediaFrame->GetSequenceNumber(),
                                                      mediaFrame->GetTimestamp(),
                                                      mediaFrame->GetAbsSendtime(),
                                                      mediaFrame->GetDuration());
            const auto ok = _segment.AddFrame(payload.data(), payload.size(), track->_number,
                                              track->GetTimestampNs(), mediaFrame->IsKeyFrame());
            if (ok) {
                track->_granule += payload.size();
                if (!GetOutputDevice()->IsFileDevice()) {
                    _segment.CuesTrack(track->_number); // for live mode
                }
            }
            GetOutputDevice()->EndWriteMediaPayload(mediaFrame->GetSsrc(), ok);
        }
    }
}

RtpAudioWebMSerializer::TrackInfo* RtpAudioWebMSerializer::GetTrack(const RtpAudioConfig* config)
{
    size_t trackKey = 0UL;
    if (config) {
        trackKey = Utils::HashCombine(config->GetSampleRate(), config->GetChannelCount(), config->GetBitsPerSample());
    }
    else {
        trackKey = Utils::HashCombine(_defaultSampleRate, _defaultChannelCount, _defaultBitDepth);
    }
    auto it = _tracks.find(trackKey);
    if (it == _tracks.end()) {
        OpusMkvCodecPrivate privateData;
        uint64_t bitDepth = _defaultBitDepth;
        if (config) {
            privateData._channelCount = config->GetChannelCount();
            privateData._sampleRate = config->GetSampleRate();
            bitDepth = config->GetBitsPerSample();
        }
        const auto trackNumber = _segment.AddAudioTrack(privateData._sampleRate,
                                                        privateData._channelCount,
                                                        static_cast<mkvmuxer::int32>(_tracks.size()) + 1);
        if (0ULL == trackNumber) {
            // log err
            return nullptr;
        }
        const auto track = static_cast<mkvmuxer::AudioTrack*>(_segment.GetTrackByNumber(trackNumber));
        if (privateData._sampleRate == 48000U) { // https://wiki.xiph.org/MatroskaOpus
            track->set_seek_pre_roll(80000000ULL);
        }
        track->set_codec_id(_opusCodec ? mkvmuxer::Tracks::kOpusCodecId : mkvmuxer::Tracks::kVorbisCodecId);
        track->set_bit_depth(bitDepth);
        track->SetCodecPrivate(reinterpret_cast<const uint8_t*>(&privateData), sizeof(privateData));
        it = _tracks.emplace(trackKey, TrackInfo(trackNumber, privateData._sampleRate)).first;
    }
    return &it->second;
}

mkvmuxer::int32 RtpAudioWebMSerializer::Write(const void* buf, mkvmuxer::uint32 len)
{
    return GetOutputDevice()->Write(buf, len) ? 0 : 1;
}

mkvmuxer::int64 RtpAudioWebMSerializer::Position() const
{
    return GetOutputDevice()->GetPosition();
}

mkvmuxer::int32 RtpAudioWebMSerializer::Position(mkvmuxer::int64 position)
{
    return GetOutputDevice()->SetPosition(position) ? 0 : 1;
}

bool RtpAudioWebMSerializer::Seekable() const
{
    return GetOutputDevice()->Seekable();
}

RtpAudioWebMSerializer::TrackInfo::TrackInfo(uint64_t number, uint32_t sampleRate)
    : _number(number)
    , _sampleRate(sampleRate)
{
}

uint64_t RtpAudioWebMSerializer::TrackInfo::GetTimestampNs() const
{
    return (_granule * 1000ULL * 1000ULL * 1000ULL) / _sampleRate;
}

} // namespace RTC
