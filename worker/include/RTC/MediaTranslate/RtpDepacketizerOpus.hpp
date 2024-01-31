#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{


class AudioFrameConfig;

class RtpDepacketizerOpus : public RtpDepacketizer
{
    class OpusHeadBuffer;
public:
    RtpDepacketizerOpus(const RtpCodecMimeType& mimeType, uint32_t sampleRate);
    ~RtpDepacketizerOpus() final;
    // impl. of RtpDepacketizer
    std::shared_ptr<const RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
private:
    std::shared_ptr<AudioFrameConfig> EnsureAudioConfig(uint8_t channelCount);
    std::shared_ptr<AudioFrameConfig> EnsureStereoAudioConfig(bool stereo);
private:
    static inline constexpr uint8_t _bitsPerSample = 16U;
    const uint32_t _sampleRate;
    std::shared_ptr<OpusHeadBuffer> _opusCodecData;
    std::shared_ptr<AudioFrameConfig> _audioConfig;
};

} // namespace RTC
