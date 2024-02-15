#pragma once
#include <memory>

namespace RTC
{

class MemoryBuffer;

class MediaSink
{
public:
    virtual ~MediaSink() = default;
    virtual void StartMediaWriting() {}
    virtual void WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer) = 0;
    virtual void EndMediaWriting() {}
};

} // namespace RTC
