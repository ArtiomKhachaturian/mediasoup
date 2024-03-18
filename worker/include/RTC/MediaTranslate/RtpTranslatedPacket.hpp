#pragma once
#include "RTC/Buffers/Buffer.hpp"
#include "RTC/MediaTranslate/Timestamp.hpp"
#include "RTC/RtpPacket.hpp"
#include <memory>

namespace RTC
{

class BufferAllocator;

class RtpTranslatedPacket
{
public:
    // [timestampOffset] from beginning of media stream, zero for 1st frame
    // [buffer] included RTP header + data
    RtpTranslatedPacket(Timestamp timestampOffset,
                        std::shared_ptr<Buffer> buffer,
                        size_t payloadOffset,
                        size_t payloadLength,
                        const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    RtpTranslatedPacket() = default;
    RtpTranslatedPacket(const RtpTranslatedPacket&) = delete;
    RtpTranslatedPacket(RtpTranslatedPacket&& tmp);
    ~RtpTranslatedPacket();
    RtpTranslatedPacket& operator = (const RtpTranslatedPacket&) = delete;
    RtpTranslatedPacket& operator = (RtpTranslatedPacket&& tmp);
    std::unique_ptr<RtpPacket> Take() { return std::move(_impl); }
    const Timestamp& GetTimestampOffset() const { return _timestampOffset; }
    void SetMarker(bool set);
    void SetSsrc(uint32_t ssrc);
    void SetPayloadType(uint8_t type);
    constexpr explicit operator bool () const { return nullptr != _impl.get(); }
private:
    Timestamp _timestampOffset;
    std::unique_ptr<RtpPacket> _impl;
};

} // namespace RTC
