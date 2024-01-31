#pragma once
#include "RTC/RtpPacket.hpp"
#include <memory>

namespace RTC
{

class MemoryBuffer;

class RtpMemoryBufferPacket : public RtpPacket
{
public:
    ~RtpMemoryBufferPacket() final;
    static RtpPacket* Create(const std::shared_ptr<const MemoryBuffer>& payload);
private:
    RtpMemoryBufferPacket(std::unique_ptr<uint8_t[]> headerBuffer,
                          const std::shared_ptr<const MemoryBuffer>& payload);
private:
    const std::unique_ptr<uint8_t[]> _headerBuffer;
    const std::shared_ptr<const MemoryBuffer> _payload;
};

} // namespace RTC
