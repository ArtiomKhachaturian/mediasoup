#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializationFactory.hpp"

//#define USE_TEST_FILE_FOR_DESERIALIZATION

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
#ifdef USE_TEST_FILE_FOR_DESERIALIZATION
private:
    static inline const char* _testFileName = "/Users/user/Downloads/1b0cefc4-abdb-48d0-9c50-f5050755be94.webm";
#endif
};

} // namespace RTC
