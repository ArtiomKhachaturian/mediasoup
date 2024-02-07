#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

// https://www.webmproject.org/docs/container/#muxer-guidelines
class WebMSerializer : public MediaFrameSerializer
{
    class BufferedWriter;
public:
    WebMSerializer(uint32_t ssrc, uint32_t clockRate, const RtpCodecMimeType& mime,
                   const char* app = "SpeakShiftSFU");
    ~WebMSerializer() final;
    static bool IsSupported(const RtpCodecMimeType& mimeType);
    static const char* GetCodecId(RtpCodecMimeType::Subtype codec);
    static const char* GetCodecId(const RtpCodecMimeType& mime);
    // impl. of MediaSource
    bool AddSink(MediaSink* sink) final;
    bool RemoveSink(MediaSink* sink) final;
    void RemoveAllSinks() final;
    bool HasSinks() const final;
    size_t GetSinksCout() const final;
    // impl. of RtpMediaFrameSerializer
    std::string_view GetFileExtension() const final { return "webm"; }
    bool Push(const std::shared_ptr<const MediaFrame>& mediaFrame) final;
private:
    std::unique_ptr<BufferedWriter> CreateWriter(MediaSink* sink) const;
    bool IsAccepted(const std::shared_ptr<const MediaFrame>& mediaFrame) const;
    uint64_t UpdateTimeStamp(uint32_t timestamp);
private:
    static inline constexpr int32_t _trackNumber = 1;
    const char* const _app;
    absl::flat_hash_map<MediaSink*, std::unique_ptr<BufferedWriter>> _sinks;
    uint32_t _lastTimestamp = 0UL;
    uint64_t _granule = 0ULL;
};

} // namespace RTC
