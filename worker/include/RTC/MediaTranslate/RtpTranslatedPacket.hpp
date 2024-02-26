#pragma once
#include "RTC/Timestamp.hpp"
#include "RTC/Buffers/Buffer.hpp"
#include <memory>

namespace RTC
{

class RtpCodecMimeType;
class RtpPacket;

class RtpTranslatedPacket
{
    class BufferedRtpPacket;
public:
    RtpTranslatedPacket() = delete;
    RtpTranslatedPacket(const RtpTranslatedPacket&) = delete;
    RtpTranslatedPacket(RtpTranslatedPacket&& tmp);
    // [timestampOffset] from beginning of media stream, zero for 1st frame
    // [buffer] included RTP header + data
    RtpTranslatedPacket(const RtpCodecMimeType& mime,
                        Timestamp timestampOffset,
                        std::shared_ptr<Buffer> buffer,
                        size_t payloadOffset, size_t payloadLength);
    ~RtpTranslatedPacket();
    RtpTranslatedPacket& operator = (const RtpTranslatedPacket&) = delete;
    RtpTranslatedPacket& operator = (RtpTranslatedPacket&& tmp);
    const Timestamp& GetTimestampOffset() const { return _timestampOffset; }
    void SetMarker(bool set);
    void SetSsrc(uint32_t ssrc);
    void SetPayloadType(uint8_t type);
private:
    Timestamp _timestampOffset;
    std::unique_ptr<RtpPacket> _impl;
};

} // namespace RTC
