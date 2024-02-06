#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

// https://www.webmproject.org/docs/container/#muxer-guidelines
class WebMSerializer : public MediaFrameSerializer, private MediaSink
{
    class BufferedWriter;
public:
    WebMSerializer(uint32_t ssrc, uint32_t clockRate, const RtpCodecMimeType& mime,
                   const char* app = "SpeakShiftSFU");
    ~WebMSerializer() final;
    static bool IsSupported(const RtpCodecMimeType& mimeType);
    static const char* GetCodecId(RtpCodecMimeType::Subtype codec);
    static const char* GetCodecId(const RtpCodecMimeType& mime);
    // impl. of RtpMediaFrameSerializer
    std::string_view GetFileExtension() const final { return "webm"; }
    bool Push(const std::shared_ptr<const MediaFrame>& mediaFrame) final;
private:
    static std::unique_ptr<BufferedWriter> CreateWriter(uint32_t ssrc,
                                                        uint32_t clockRate,
                                                        const RtpCodecMimeType& mime,
                                                        MediaSink* sink,
                                                        const char* app);
    bool IsAccepted(const std::shared_ptr<const MediaFrame>& mediaFrame) const;
    uint64_t UpdateTimeStamp(uint32_t timestamp);
    // impl. of MediaSink
    void StartMediaWriting(uint32_t ssrc) final;
    void WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer) final;
    void EndMediaWriting() final;
private:
    static inline constexpr int32_t _trackNumber = 1;
    const std::unique_ptr<BufferedWriter> _writer;
    uint32_t _lastTimestamp = 0UL;
    uint64_t _granule = 0ULL;
};

} // namespace RTC
