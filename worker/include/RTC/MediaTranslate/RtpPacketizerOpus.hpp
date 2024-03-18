#pragma once
#include "RTC/MediaTranslate/RtpPacketizer.hpp"

namespace RTC
{

class RtpPacketizerOpus : public RtpPacketizer
{
public:
    RtpPacketizerOpus(const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    // impl. of RtpPacketizer
    RtpTranslatedPacket Add(size_t payloadOffset, size_t payloadLength,
                            MediaFrame&& frame) final;
private:
    bool _firstFrame = true;
};

} // namespace RTC
