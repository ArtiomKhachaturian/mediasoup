#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerOpus : public RtpDepacketizer
{
    class OpusHeadBuffer;
public:
    RtpDepacketizerOpus(const RtpCodecMimeType& codecMimeType, uint32_t sampleRate);
    ~RtpDepacketizerOpus() final;
    // impl. of RtpDepacketizer
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
private:
    const uint32_t _sampleRate;
    std::shared_ptr<OpusHeadBuffer> _opusCodecData;
    std::allocator<uint8_t> _payloadAllocator;
};

} // namespace RTC
