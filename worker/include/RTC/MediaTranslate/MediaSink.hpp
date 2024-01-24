#pragma once
#include <memory>

namespace RTC
{

class MemoryBuffer;

class MediaSink
{
public:
    virtual ~MediaSink() = default;
    virtual void StartMediaWriting(bool /*restart*/, uint32_t /*startTimestamp*/) noexcept {}
    virtual void WriteMediaPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept = 0;
    virtual void EndMediaWriting() noexcept {}
};

} // namespace RTC
