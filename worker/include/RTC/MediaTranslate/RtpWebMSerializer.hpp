#ifndef MS_RTC_WEBM_SERIALIZER_HPP
#define MS_RTC_WEBM_SERIALIZER_HPP

#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include <mkvmuxer/mkvmuxer.h>
#include <unordered_map>

namespace RTC
{

class RtpMediaFrame;
class RtpCodecMimeType;
class RtpVideoConfig;
class RtpAudioConfig;

class RtpWebMSerializer : public RtpMediaFrameSerializer, private mkvmuxer::IMkvWriter
{
    struct TrackInfo;
public:
    // OPUS or VORBIS serializer
    RtpWebMSerializer();
    ~RtpWebMSerializer() final;
    static bool IsSupported(const RtpCodecMimeType& mimeType);
    // impl. of RtpMediaFrameSerializer
    void SetOutputDevice(OutputDevice* outputDevice) final;
    void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) final;
private:
    static const char* GetCodec(const RtpCodecMimeType& mimeType);
    static bool IsOpusAudio(const RtpCodecMimeType& mimeType);
    static bool SetupTrack(mkvmuxer::AudioTrack* track, const RtpAudioConfig* config,
                           const RtpCodecMimeType& mimeType);
    static bool SetupTrack(mkvmuxer::VideoTrack* track, const RtpVideoConfig* config,
                           const RtpCodecMimeType& mimeType);
    bool SetupTrack(uint64_t trackNumber, const RtpAudioConfig* config, const RtpCodecMimeType& mimeType);
    bool SetupTrack(uint64_t trackNumber, const RtpVideoConfig* config, const RtpCodecMimeType& mimeType);
    TrackInfo* GetTrack(const std::shared_ptr<RtpMediaFrame>& mediaFrame);
    // impl. of mkvmuxer::IMkvWriter
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final;
    mkvmuxer::int64 Position() const final;
    mkvmuxer::int32 Position(mkvmuxer::int64) final;
    bool Seekable() const final;
    void ElementStartNotify(mkvmuxer::uint64, mkvmuxer::int64) final {}
private:
    mkvmuxer::Segment _segment;
    std::unordered_map<size_t, TrackInfo> _tracks;
};

} // namespace RTC

#endif
