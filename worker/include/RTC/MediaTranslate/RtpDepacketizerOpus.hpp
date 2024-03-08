#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerOpus : public RtpAudioDepacketizer
{
    class OpusHeadBuffer;
public:
    RtpDepacketizerOpus(uint32_t clockRate, bool multiopus = false,
                        const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    ~RtpDepacketizerOpus() final;
    // impl. of RtpDepacketizer
    std::optional<MediaFrame> AddPacket(const RtpPacket* packet,
                                        bool makeDeepCopyOfPayload,
                                        bool* configWasChanged) final;
    AudioFrameConfig GetAudioConfig(const RtpPacket*) const final { return _config; }
private:
    static inline constexpr uint8_t _bitsPerSample = 16U;
    std::shared_ptr<OpusHeadBuffer> _opusCodecData;
    AudioFrameConfig _config;
};

} // namespace RTC
