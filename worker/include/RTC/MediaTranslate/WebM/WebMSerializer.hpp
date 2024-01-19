#pragma once

#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class RtpCodecMimeType;

// https://www.webmproject.org/docs/container/#muxer-guidelines
class WebMSerializer : public MediaFrameSerializer
{
    class BufferedWriter;
    class TrackInfo;
public:
    WebMSerializer(const char* writingApp = "SpeakShiftSFU");
    ~WebMSerializer() final;
    static bool IsSupported(const RtpCodecMimeType& mimeType);
    static const char* GetCodecId(RtpCodecMimeType::Subtype codec);
    static const char* GetCodecId(const RtpCodecMimeType& mime);
    // impl. of RtpMediaFrameSerializer
    void SetLiveMode(bool liveMode) final;
    std::string_view GetFileExtension(const RtpCodecMimeType& mimeType) const final;
    bool AddAudio(uint32_t ssrc, uint32_t clockRate,
                  RtpCodecMimeType::Subtype codec,
                  const std::shared_ptr<const AudioFrameConfig>& config) final;
    bool AddVideo(uint32_t ssrc, uint32_t clockRate,
                  RtpCodecMimeType::Subtype codec,
                  const std::shared_ptr<const VideoFrameConfig>& config) final;
    void RemoveMedia(uint32_t ssrc) final;
    bool Push(uint32_t ssrc, const std::shared_ptr<const MediaFrame>& mediaFrame) final;
    bool IsCompatible(const RtpCodecMimeType& mimeType) const final;
private:
    void InitWriter();
    void DestroyWriter(bool failure = false);
    template<class TConfig>
    bool AddMedia(uint32_t ssrc, uint32_t clockRate, const RtpCodecMimeType& mime, 
                  const std::shared_ptr<TConfig>& config);
    TrackInfo* GetTrackInfo(uint32_t ssrc, const std::shared_ptr<const MediaFrame>& mediaFrame) const;
private:
    const char* const _writingApp;
    std::unique_ptr<BufferedWriter> _writer;
    bool _liveMode = true;
    // key - is packet SSRC
    absl::flat_hash_map<uint32_t, std::unique_ptr<TrackInfo>> _tracksInfo;
    bool _pendingRestartMode = false;
    bool _hasFailure = false;
};

} // namespace RTC
