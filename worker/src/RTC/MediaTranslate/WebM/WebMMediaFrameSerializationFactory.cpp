#include "RTC/MediaTranslate/WebM/WebMMediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"

namespace RTC
{

std::unique_ptr<MediaFrameSerializer> WebMMediaFrameSerializationFactory::
    CreateSerializer(uint32_t ssrc, uint32_t clockRate, const RtpCodecMimeType& mime)
{
    if (WebMSerializer::IsSupported(mime)) {
        return std::make_unique<WebMSerializer>(ssrc, clockRate, mime);
    }
    return nullptr;
}

std::unique_ptr<MediaFrameDeserializer> WebMMediaFrameSerializationFactory::CreateDeserializer()
{
    return std::make_unique<WebMDeserializer>();
}

} // namespace RTC
