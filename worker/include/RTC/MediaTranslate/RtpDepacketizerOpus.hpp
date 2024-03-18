#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerOpus : public RtpAudioDepacketizer
{
    class OpusHeadBuffer;
public:
    RtpDepacketizerOpus(uint32_t ssrc, uint32_t clockRate,
                        const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    ~RtpDepacketizerOpus() final;
    // impl. of RtpDepacketizer
    MediaFrame AddPacketInfo(uint32_t rtpTimestamp,
                             bool keyFrame, bool hasMarker,
                             const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                             const std::shared_ptr<Buffer>& payload,
                             bool* configWasChanged) final;
    AudioFrameConfig GetAudioConfig() const final { return _config; }
private:
    static inline constexpr uint8_t _bitsPerSample = 16U;
    std::shared_ptr<OpusHeadBuffer> _opusCodecData;
    AudioFrameConfig _config;
};

} // namespace RTC
