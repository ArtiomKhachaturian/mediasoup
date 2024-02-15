#pragma once
#include <memory>

namespace RTC
{

class MemoryBuffer;
class MediaObject;

class MediaSink
{
public:
    virtual ~MediaSink() = default;
    virtual void StartMediaWriting(const MediaObject& /*sender*/) {}
    virtual void WriteMediaPayload(const MediaObject& sender, const std::shared_ptr<MemoryBuffer>& buffer) = 0;
    virtual void EndMediaWriting(const MediaObject& /*sender*/) {}
};

} // namespace RTC
