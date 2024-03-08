#pragma once
#include <memory>

namespace RTC
{

class Buffer;

class MediaSink
{
public:
    virtual ~MediaSink() = default;
    virtual void StartMediaWriting(uint64_t /*senderId*/) {}
    virtual void WriteMediaPayload(uint64_t senderId, const std::shared_ptr<Buffer>& buffer) = 0;
    virtual void EndMediaWriting(uint64_t /*senderId*/) {}
};

} // namespace RTC
