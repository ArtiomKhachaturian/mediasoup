#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{


class AudioFrameConfig;

class RtpDepacketizerOpus : public RtpDepacketizer
{
    class OpusHeadBuffer;
public:
    RtpDepacketizerOpus(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                        const std::shared_ptr<BufferAllocator>& allocator);
    ~RtpDepacketizerOpus() final;
    // impl. of RtpDepacketizer
    std::shared_ptr<MediaFrame> AddPacket(const RtpPacket* packet, bool makeDeepCopyOfPayload) final;
private:
    std::shared_ptr<AudioFrameConfig> EnsureAudioConfig(uint8_t channelCount);
    std::shared_ptr<AudioFrameConfig> EnsureStereoAudioConfig(bool stereo);
private:
    static inline constexpr uint8_t _bitsPerSample = 16U;
    std::shared_ptr<OpusHeadBuffer> _opusCodecData;
    std::shared_ptr<AudioFrameConfig> _audioConfig;
};

} // namespace RTC
