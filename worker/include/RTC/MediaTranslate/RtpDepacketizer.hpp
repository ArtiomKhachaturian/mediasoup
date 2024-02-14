#pragma once

#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

class RtpMediaFrame;
class RtpMediaFrameSerializer;
class RtpPacket;

class RtpDepacketizer
{
public:
    virtual ~RtpDepacketizer() = default;
    virtual std::shared_ptr<const RtpMediaFrame> AddPacket(const RtpPacket* packet) = 0;
    const RtpCodecMimeType& GetMimeType() const { return _mimeType; }
    uint32_t GetClockRate() const { return _clockRate; }
    static std::unique_ptr<RtpDepacketizer> Create(const RtpCodecMimeType& mimeType, uint32_t clockRate);
protected:
    RtpDepacketizer(const RtpCodecMimeType& mimeType, uint32_t clockRate);
    const std::allocator<uint8_t>& GetPayloadAllocator() const { return _payloadAllocator; }
private:
    const RtpCodecMimeType _mimeType;
    const uint32_t _clockRate;
    const std::allocator<uint8_t> _payloadAllocator;
};

} // namespace RTC
