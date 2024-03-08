#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"

namespace RTC
{

class WebMSerializer : public MediaFrameSerializer
{
    class Writer;
public:
    WebMSerializer(const RtpCodecMimeType& mime, uint32_t clockRate,
                   const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    ~WebMSerializer() final = default;
protected:
    // impl. of MediaFrameSerializer
    std::string_view GetFileExtension() const final { return "webm"; }
    std::unique_ptr<MediaFrameWriter> CreateWriter(uint64_t senderId,
                                                   MediaSink* sink) final;
};

} // namespace RTC
