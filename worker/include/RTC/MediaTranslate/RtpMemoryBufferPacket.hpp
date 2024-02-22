#pragma once
#include "RTC/RtpPacket.hpp"
#include <memory>

namespace RTC
{

class Buffer;

class RtpMemoryBufferPacket : public RtpPacket
{
public:
    ~RtpMemoryBufferPacket() final;
    static RtpPacket* Create(const std::shared_ptr<const Buffer>& payload);
private:
    RtpMemoryBufferPacket(std::unique_ptr<uint8_t[]> headerBuffer,
                          const std::shared_ptr<const Buffer>& payload);
private:
    const std::unique_ptr<uint8_t[]> _headerBuffer;
    const std::shared_ptr<const Buffer> _payload;
};

} // namespace RTC
