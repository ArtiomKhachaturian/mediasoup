#pragma once
#include <memory>

namespace RTC
{

class MediaFrameSerializer;
class MediaFrameDeserializer;

class MediaFrameSerializationFactory
{
public:
    virtual std::unique_ptr<MediaFrameSerializer> CreateSerializer() = 0;
    virtual std::unique_ptr<MediaFrameDeserializer> CreateDeserializer() = 0;
protected:
    virtual ~MediaFrameSerializationFactory() = default;
};

} // namespace RTC