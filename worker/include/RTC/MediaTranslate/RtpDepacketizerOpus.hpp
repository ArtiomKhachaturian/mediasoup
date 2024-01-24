#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

namespace Codecs {
    class EncodingContext;
}

class RtpDepacketizerOpus : public RtpDepacketizer
{
    class OpusHeadBuffer;
public:
    RtpDepacketizerOpus(const RtpCodecMimeType& mimeType, uint32_t sampleRate);
    ~RtpDepacketizerOpus() final;
    // impl. of RtpDepacketizer
    std::shared_ptr<const RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
private:
    bool IsValidPacket(const RtpPacket* packet) const;
private:
    const uint32_t _sampleRate;
    const std::unique_ptr<Codecs::EncodingContext> _encodingContext;
    std::shared_ptr<OpusHeadBuffer> _opusCodecData;
};

} // namespace RTC
