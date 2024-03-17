#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/MkvBufferedReader.hpp"

namespace RTC
{

class WebMDeserializer : public MediaFrameDeserializer
{
public:
    WebMDeserializer(const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    ~WebMDeserializer() final;
    void Clear() final;
protected:
    // impl. of MediaFrameDeserializer
    MediaFrameDeserializeResult AddBuffer(std::shared_ptr<Buffer> buffer) final;
    void ParseTracksInfo() final;
private:
    MkvBufferedReader _reader;
};

} // namespace RTC
