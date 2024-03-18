#pragma once
#include <cstdint>
#include <memory>

namespace RTC
{

namespace Codecs {
class PayloadDescriptorHandler;
}
class BufferAllocator;
class Buffer;
class RtpPacket;


struct RtpPacketInfo
{
    RtpPacketInfo() = default;
    RtpPacketInfo(uint32_t timestamp, bool keyFrame, bool hasMarker,
           	      std::shared_ptr<const Codecs::PayloadDescriptorHandler> pdh,
           		  std::shared_ptr<Buffer> payload);
    RtpPacketInfo(const RtpPacketInfo&) = default;
    RtpPacketInfo(RtpPacketInfo&&) = default;
    RtpPacketInfo& operator = (const RtpPacketInfo&) = delete;
    RtpPacketInfo& operator = (RtpPacketInfo&&) = delete;
    static RtpPacketInfo FromRtpPacket(const RtpPacket* packet,
                                       const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    const uint32_t _timestamp = 0U; // RTP time
    const bool _keyFrame = false;
    const bool _hasMarker = false;
    const std::shared_ptr<const Codecs::PayloadDescriptorHandler> _pdh;
    const std::shared_ptr<Buffer> _payload;
};

inline RtpPacketInfo::RtpPacketInfo(uint32_t timestamp,
									bool keyFrame, bool hasMarker,
                                    std::shared_ptr<const Codecs::PayloadDescriptorHandler> pdh,
                                    std::shared_ptr<Buffer> payload)
    : _timestamp(timestamp)
    , _keyFrame(keyFrame)
    , _hasMarker(hasMarker)
    , _pdh(std::move(pdh))
    , _payload(std::move(payload))
{
}

} // namespace RTC
