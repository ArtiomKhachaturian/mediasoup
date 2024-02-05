#pragma once
#include <memory>

namespace RTC
{

class MediaFrameSerializer;
class MediaFrameDeserializer;
class RtpCodecMimeType;

class MediaFrameSerializationFactory
{
public:
    // timeSliceMs is number of milliseconds to record into each BLOB.
    // See also https://developer.mozilla.org/en-US/docs/Web/API/MediaRecorder/start
    virtual std::unique_ptr<MediaFrameSerializer> CreateSerializer(uint32_t ssrc,
                                                                   uint32_t clockRate,
                                                                   const RtpCodecMimeType& mime,
                                                                   uint32_t timeSliceMs = 200U) = 0;
    virtual std::unique_ptr<MediaFrameDeserializer> CreateDeserializer() = 0;
protected:
    virtual ~MediaFrameSerializationFactory() = default;
};

} // namespace RTC
