#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"

namespace RTC
{

class WebMSerializer : public BufferAllocations<MediaFrameSerializer>
{
    class Writer;
public:
    WebMSerializer(const RtpCodecMimeType& mime,
                   const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    ~WebMSerializer() final = default;
protected:
    // impl. of MediaFrameSerializer
    std::string_view GetFileExtension() const final { return "webm"; }
    std::unique_ptr<MediaFrameWriter> CreateWriter(MediaSink* sink);
};

} // namespace RTC
