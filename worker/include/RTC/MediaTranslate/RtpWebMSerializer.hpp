#pragma once

#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include <mkvmuxer/mkvmuxer.h>
#include <unordered_map>

namespace RTC
{

class RtpMediaFrame;
class RtpCodecMimeType;

class RtpWebMSerializer : public RtpMediaFrameSerializer
{
    class BufferedWriter;
    struct TrackInfo;
public:
    // OPUS or VORBIS serializer
    RtpWebMSerializer();
    ~RtpWebMSerializer() final;
    static bool IsSupported(const RtpCodecMimeType& mimeType);
    // impl. of RtpMediaFrameSerializer
    void SetOutputDevice(OutputDevice* outputDevice) final;
    void SetLiveMode(bool liveMode) final;
    std::string_view GetFileExtension(const RtpCodecMimeType& mimeType) const final;
    bool IsCompatible(const RtpCodecMimeType& mimeType) const final;
    void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) final;
private:
    static const char* GetCodec(const RtpCodecMimeType& mimeType);
    static bool IsOpusAudio(const RtpCodecMimeType& mimeType);
    TrackInfo* GetTrack(const std::shared_ptr<RtpMediaFrame>& mediaFrame);
    void CommitData(OutputDevice* outputDevice);
private:
    const std::unique_ptr<BufferedWriter> _writer;
    mkvmuxer::Segment _segment;
    // key - is packet SSRC
    std::unordered_map<uint32_t, TrackInfo> _tracks;
};

} // namespace RTC
