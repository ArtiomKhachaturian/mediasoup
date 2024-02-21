#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "absl/container/flat_hash_map.h"


namespace RTC
{

// https://www.webmproject.org/docs/container/#muxer-guidelines
class WebMSerializer : public MediaFrameSerializer
{
    class Writer;
public:
    WebMSerializer(const RtpCodecMimeType& mime);
    ~WebMSerializer() final = default;
protected:
    // impl. of MediaFrameSerializer
    std::string_view GetFileExtension() const final { return "webm"; }
    std::unique_ptr<MediaFrameWriter> CreateWriter(MediaSink* sink);
};

} // namespace RTC
