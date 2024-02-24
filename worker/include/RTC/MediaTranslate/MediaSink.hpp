#pragma once
#include <memory>

namespace RTC
{

class Buffer;
class ObjectId;

class MediaSink
{
public:
    virtual ~MediaSink() = default;
    virtual void StartMediaWriting(const ObjectId& /*sender*/) {}
    virtual void WriteMediaPayload(const ObjectId& sender, const std::shared_ptr<Buffer>& buffer) = 0;
    virtual void EndMediaWriting(const ObjectId& /*sender*/) {}
};

} // namespace RTC
