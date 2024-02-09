#pragma once
#include <memory>

namespace RTC
{

class MemoryBuffer;

class MediaSink
{
public:
    virtual ~MediaSink() = default;
    virtual void StartMediaWriting(uint32_t /*ssrc*/) {}
    virtual void WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<MemoryBuffer>& buffer) = 0;
    virtual void EndMediaWriting(uint32_t /*ssrc*/) {}
};

} // namespace RTC
