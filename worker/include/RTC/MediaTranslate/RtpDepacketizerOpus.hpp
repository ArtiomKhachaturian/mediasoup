#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerOpus : public RtpDepacketizer
{
    class OpusHeadBuffer;
public:
    RtpDepacketizerOpus(const RtpCodecMimeType& mimeType, uint32_t sampleRate);
    ~RtpDepacketizerOpus() final;
    // impl. of RtpDepacketizer
    std::shared_ptr<const RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
private:
    const uint32_t _sampleRate;
    std::shared_ptr<OpusHeadBuffer> _opusCodecData;
};

} // namespace RTC
