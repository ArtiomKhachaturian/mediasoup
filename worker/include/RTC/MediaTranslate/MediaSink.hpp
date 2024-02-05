#pragma once
#include <memory>

namespace RTC
{

class MemoryBuffer;

class MediaSink
{
public:
    virtual ~MediaSink() = default;
    virtual bool IsLiveMode() const = 0;
    virtual void StartMediaWriting(uint32_t /*ssrc*/) {}
    virtual void WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer) = 0;
    virtual void EndMediaWriting() {}
};

} // namespace RTC
