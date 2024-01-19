#pragma once
#include <memory>

namespace RTC
{

class MemoryBuffer;

class MediaSink
{
public:
    virtual ~MediaSink() = default;
    virtual void StartStream(bool /*restart*/) noexcept {}
    virtual void BeginWriteMediaPayload(uint32_t /*ssrc*/) noexcept {}
    virtual void WriteMediaPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept = 0;
    virtual void EndWriteMediaPayload(uint32_t /*ssrc*/, bool ok) noexcept {}
    virtual void EndStream(bool /*failure*/) noexcept {}
};

} // namespace RTC
