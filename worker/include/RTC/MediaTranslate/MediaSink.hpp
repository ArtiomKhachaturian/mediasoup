#pragma once
#include <memory>

namespace RTC
{

class MemoryBuffer;

class MediaSink
{
public:
    virtual ~MediaSink() = default;
    virtual void StartMediaWriting(bool /*restart*/) {}
    virtual void WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<const MemoryBuffer>& buffer) = 0;
    virtual void EndMediaWriting() {}
};

} // namespace RTC
