#pragma once
#include "RTC/MediaTranslate/MediaFrameSerializationFactory.hpp"

namespace RTC
{

class WebMMediaFrameSerializationFactory : public MediaFrameSerializationFactory
{
public:
    WebMMediaFrameSerializationFactory() = default;
    ~WebMMediaFrameSerializationFactory() final = default;
    // impl. of MediaFrameSerializationFactory
    std::unique_ptr<MediaFrameSerializer> CreateSerializer(uint32_t ssrc, uint32_t clockRate,
                                                           const RtpCodecMimeType& mime) final;
    std::unique_ptr<MediaFrameDeserializer> CreateDeserializer() final;
};

} // namespace RTC
