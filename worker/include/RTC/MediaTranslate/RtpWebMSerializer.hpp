#pragma once

#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include <mkvmuxer/mkvmuxer.h>
#include <unordered_map>

namespace RTC
{

class RtpMediaFrame;
class RtpCodecMimeType;
class RtpVideoConfig;
class RtpAudioConfig;

class RtpWebMSerializer : public RtpMediaFrameSerializer
{
    class BufferedWriter;
    class MediaBufferImpl;
    struct TrackInfo;
public:
    // OPUS or VORBIS serializer
    RtpWebMSerializer();
    ~RtpWebMSerializer() final;
    static bool IsSupported(const RtpCodecMimeType& mimeType);
    // impl. of RtpMediaFrameSerializer
    void SetOutputDevice(OutputDevice* outputDevice) final;
    void SetLiveMode(bool liveMode) final;
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
    void CommitData(OutputDevice* outputDevice);
private:
    const std::unique_ptr<BufferedWriter> _writer;
    mkvmuxer::Segment _segment;
    std::unordered_map<size_t, TrackInfo> _tracks;
};

} // namespace RTC
