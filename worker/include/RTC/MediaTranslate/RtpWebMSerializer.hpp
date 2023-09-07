#pragma once

#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include <mkvmuxer/mkvmuxer.h>
#include <absl/container/flat_hash_map.h>

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
    bool RegisterAudio(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                       const RtpAudioFrameConfig* config) final;
    bool RegisterVideo(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                       const RtpVideoFrameConfig* config) final;
    void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) final;
    bool IsCompatible(const RtpCodecMimeType& mimeType) const final;
private:
    template<class TMkvMediaTrack, class TConfig>
    bool RegisterMedia(uint32_t ssrc, RtpCodecMimeType::Subtype codec, const TConfig* config);
    mkvmuxer::Track* AddMediaTrack(uint32_t ssrc, const RtpAudioFrameConfig* config);
    mkvmuxer::Track* AddMediaTrack(uint32_t ssrc, const RtpVideoFrameConfig* config);
    TrackInfo* GetTrackInfo(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) const;
    //mkvmuxer::Track* CreateMediaTrack(const std::shared_ptr<const RtpMediaFrame>& mediaFrame);
    void CommitData(OutputDevice* outputDevice);
private:
    const std::unique_ptr<BufferedWriter> _writer;
    mkvmuxer::Segment _segment;
    // key - is packet SSRC
    absl::flat_hash_map<uint32_t, std::unique_ptr<TrackInfo>> _tracksInfo;
};

} // namespace RTC
