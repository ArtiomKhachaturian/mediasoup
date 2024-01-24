#pragma once
#include "RTC/RtpPacket.hpp"
#include <memory>

namespace RTC
{

class MemoryBuffer;

class RtpMemoryBufferPacket : public RtpPacket
{
public:
    RtpMemoryBufferPacket(Header* header,
                          HeaderExtension* headerExtension,
                          const uint8_t* payload,
                          size_t payloadLength,
                          const std::shared_ptr<const MemoryBuffer>& buffer);
    ~RtpMemoryBufferPacket() final;
    static RtpPacket* Create(const std::shared_ptr<const MemoryBuffer>& buffer);
    static size_t GetPayloadOffset() { return RtpPacket::HeaderSize; }
private:
    const std::shared_ptr<const MemoryBuffer> _buffer;
};

} // namespace RTC
