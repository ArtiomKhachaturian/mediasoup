#pragma once

#include "RTC/MediaTranslate/RtpMediaPacketInfo.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class RtpCodecMimeType;
class MemoryBuffer;

class OutputDevice
{
public:
    virtual ~OutputDevice() = default;
    virtual void StartStream(bool /*restart*/) noexcept {}
    virtual void BeginWriteMediaPayload(uint32_t /*ssrc*/,
                                        const std::vector<RtpMediaPacketInfo>& /*packets*/) noexcept {}
    virtual void Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept = 0;
    virtual void EndWriteMediaPayload(uint32_t /*ssrc*/,
                                      const std::vector<RtpMediaPacketInfo>& /*packets*/,
                                      bool ok) noexcept {}
    virtual void EndStream(bool /*failure*/) noexcept {}
};

} // namespace RTC
