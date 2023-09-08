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
    static std::unique_ptr<RtpDepacketizer> create(const RtpCodecMimeType& mimeType,
                                                   uint32_t sampleRate);
protected:
    RtpDepacketizer(const RtpCodecMimeType& mimeType);
    const std::allocator<uint8_t>& GetPayloadAllocator() const { return _payloadAllocator; }
private:
    const RtpCodecMimeType _mimeType;
    const std::allocator<uint8_t> _payloadAllocator;
};

} // namespace RTC
