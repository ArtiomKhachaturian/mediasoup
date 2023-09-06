#pragma once

#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include <mkvmuxer/mkvmuxer.h>

namespace RTC
{

class RtpMediaFrame;
class RtpCodecMimeType;

// https://www.webmproject.org/docs/container/#muxer-guidelines
class RtpWebMSerializer : public RtpMediaFrameSerializer
{
    class BufferedWriter;
    class TrackInfo;
public:
    // OPUS or VORBIS serializer
    RtpWebMSerializer();
    ~RtpWebMSerializer() final;
    static bool IsSupported(const RtpCodecMimeType& mimeType);
    // impl. of RtpMediaFrameSerializer
    void SetOutputDevice(OutputDevice* outputDevice) final;
    void SetLiveMode(bool liveMode) final;
    std::string_view GetFileExtension(const RtpCodecMimeType& mimeType) const final;
    void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) final;
private:
    TrackInfo* GetTrackInfo(const std::shared_ptr<const RtpMediaFrame>& mediaFrame);
    mkvmuxer::Track* CreateMediaTrack(const std::shared_ptr<const RtpMediaFrame>& mediaFrame);
    void CommitData(OutputDevice* outputDevice);
private:
    static inline constexpr mkvmuxer::int32 _audioTrackId = 1;
    static inline constexpr mkvmuxer::int32 _videoTrackId = 2;
    const std::unique_ptr<BufferedWriter> _writer;
    mkvmuxer::Segment _segment;
    // key - is packet SSRC
    std::unique_ptr<TrackInfo> _audioTrackInfo;
    std::unique_ptr<TrackInfo> _videoTrackInfo;
    bool _hasAudioTrackCreationError = false;
    bool _hasVideoTrackCreationError = false;
};

} // namespace RTC
