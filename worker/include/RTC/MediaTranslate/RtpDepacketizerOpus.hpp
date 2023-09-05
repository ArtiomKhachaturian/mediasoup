#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerOpus : public RtpDepacketizer
{
    class TimeStampProviderImpl;
public:
    RtpDepacketizerOpus(const RtpCodecMimeType& codecMimeType, uint32_t sampleRate);
    // impl. of RtpDepacketizer
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
private:
    const std::shared_ptr<TimeStampProviderImpl> _timeStampProvider;
    std::allocator<uint8_t> _payloadAllocator;
};

} // namespace RTC
