#include "RTC/MediaTranslate/WebM/WebMMediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMBuffersReader.hpp"

namespace RTC
{

std::unique_ptr<MediaFrameSerializer> WebMMediaFrameSerializationFactory::CreateSerializer()
{
    return std::make_unique<WebMSerializer>();
}

std::unique_ptr<MediaFrameDeserializer> WebMMediaFrameSerializationFactory::CreateDeserializer()
{
    return std::make_unique<WebMDeserializer>(std::make_unique<WebMBuffersReader>());
}

} // namespace RTC
