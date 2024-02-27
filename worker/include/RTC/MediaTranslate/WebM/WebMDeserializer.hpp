#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"

namespace RTC
{

class MkvBufferedReader;
enum class MkvReadResult;

class WebMDeserializer : public MediaFrameDeserializer
{
    class TrackInfo;
public:
    WebMDeserializer(const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    ~WebMDeserializer() final;
    void Clear() final;
protected:
    // impl. of MediaFrameDeserializer
    MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<Buffer>& buffer) final;
    void ParseTracksInfo() final;
private:
    static MediaFrameDeserializeResult FromMkvReadResult(MkvReadResult result);
private:
    const std::unique_ptr<MkvBufferedReader> _reader;
};

} // namespace RTC
