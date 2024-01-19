#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializationFactory.hpp"

#define USE_TEST_FILE_FOR_DESERIALIZATION

namespace RTC
{

class WebMMediaFrameSerializationFactory : public MediaFrameSerializationFactory
{
public:
    WebMMediaFrameSerializationFactory() = default;
    ~WebMMediaFrameSerializationFactory() final = default;
    // impl. of MediaFrameSerializationFactory
    std::unique_ptr<MediaFrameSerializer> CreateSerializer() final;
    std::unique_ptr<MediaFrameDeserializer> CreateDeserializer() final;
};

} // namespace RTC
