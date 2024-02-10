#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

// https://www.webmproject.org/docs/container/#muxer-guidelines
class WebMSerializer : public MediaFrameSerializer
{
    class Writer;
public:
    WebMSerializer(uint32_t ssrc, uint32_t clockRate, const RtpCodecMimeType& mime,
                   const char* app = "SpeakShiftSFU");
    ~WebMSerializer() final;
    // impl. of MediaSource
    bool AddSink(MediaSink* sink) final;
    bool RemoveSink(MediaSink* sink) final;
    void RemoveAllSinks() final;
    bool HasSinks() const final;
    size_t GetSinksCout() const final;
    // impl. of RtpMediaFrameSerializer
    std::string_view GetFileExtension() const final { return "webm"; }
    bool Push(const std::shared_ptr<const MediaFrame>& mediaFrame) final;
    bool AddTestSink(MediaSink* sink) final;
    bool RemoveTestSink() final;
private:
    std::unique_ptr<Writer> CreateWriter(MediaSink* sink) const;
    bool IsAccepted(const std::shared_ptr<const MediaFrame>& mediaFrame) const;
    uint64_t UpdateTimeStamp(uint32_t timestamp);
    bool Write(const std::shared_ptr<const MediaFrame>& mediaFrame,
               uint64_t mkvTimestamp, Writer* writer) const;
private:
    const char* const _app;
    absl::flat_hash_map<MediaSink*, std::unique_ptr<Writer>> _writers;
    std::unique_ptr<Writer> _testWriter;
    uint32_t _lastTimestamp = 0UL;
    uint64_t _granule = 0ULL;
};

} // namespace RTC
