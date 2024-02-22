#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "absl/container/flat_hash_map.h"


namespace RTC
{

class BufferAllocator;

class WebMSerializer : public MediaFrameSerializer
{
    class Writer;
public:
    WebMSerializer(const RtpCodecMimeType& mime, const std::weak_ptr<BufferAllocator>& allocator);
    ~WebMSerializer() final = default;
protected:
    // impl. of MediaFrameSerializer
    std::string_view GetFileExtension() const final { return "webm"; }
    std::unique_ptr<MediaFrameWriter> CreateWriter(MediaSink* sink);
private:
    const std::weak_ptr<BufferAllocator> _allocator;
};

} // namespace RTC
