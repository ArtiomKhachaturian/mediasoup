#ifndef MS_RTC_AUDIO_WEBM_SERIALIZER_HPP
#define MS_RTC_AUDIO_WEBM_SERIALIZER_HPP

#include "RTC/RtpMediaFrameSerializer.hpp"
#include <mkvmuxer/mkvmuxer.h>
#include <unordered_map>

namespace RTC
{

class RtpMediaFrame;
class RtpAudioConfig;

class RtpAudioWebMSerializer : public RtpMediaFrameSerializer,
                               private mkvmuxer::IMkvWriter
{
    struct OpusMkvCodecPrivate;
    struct TrackInfo;
public:
    // OPUS or VORBIS serializer
    RtpAudioWebMSerializer(OutputDevice* outputDevice, bool opusCodec = true);
    ~RtpAudioWebMSerializer() final;
    // impl. of RtpMediaFrameSerializer
    void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) final;
private:
    TrackInfo* GetTrack(const RtpAudioConfig* config = nullptr);
    // impl. of mkvmuxer::IMkvWriter
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final;
    mkvmuxer::int64 Position() const final;
    mkvmuxer::int32 Position(mkvmuxer::int64) final;
    bool Seekable() const final;
    void ElementStartNotify(mkvmuxer::uint64, mkvmuxer::int64) final {}
private:
    static inline constexpr uint8_t _defaultChannelCount = 1U;
    static inline constexpr uint32_t _defaultSampleRate = 48000U;
    static inline constexpr uint64_t _defaultBitDepth = 16ULL;
    const bool _opusCodec;
    mkvmuxer::Segment _segment;
    std::unordered_map<size_t, TrackInfo> _tracks;
};

} // namespace RTC

#endif
