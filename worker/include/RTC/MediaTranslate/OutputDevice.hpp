#pragma once

#include <memory>

namespace RTC
{

class RtpCodecMimeType;
class MemoryBuffer;

class OutputDevice
{
public:
    virtual ~OutputDevice() = default;
    virtual void StartStream(bool /*restart*/) noexcept {}
    virtual void BeginWriteMediaPayload(uint32_t /*ssrc*/, bool /*isKeyFrame*/,
                                        const RtpCodecMimeType& /*codecMimeType*/,
                                        uint16_t /*rtpSequenceNumber*/,
                                        uint32_t /*rtpTimestamp*/,
                                        uint32_t /*rtpAbsSendtime*/ = 0U) noexcept {}
    virtual void Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept = 0;
    virtual void EndWriteMediaPayload(uint32_t /*ssrc*/, bool /*ok*/) noexcept {}
    virtual void EndStream(bool /*failure*/) noexcept {}
};

} // namespace RTC
