#pragma once

#include "RTC/MediaTranslate/RtpMediaFrame.hpp"

namespace RTC
{

class RtpAudioFrame : public RtpMediaFrame
{
public:
    RtpAudioFrame(const RtpCodecMimeType& codecMimeType,
                  const std::shared_ptr<const MemoryBuffer>& payload,
                  bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                  uint16_t sequenceNumber, uint32_t sampleRate,
                  const RtpAudioFrameConfig& audioConfig,
                  uint32_t durationMs = 0U);
    // overrides of RtpMediaFrame
    const RtpAudioFrameConfig* GetAudioConfig() const final { return &_audioConfig; }
private:
    const RtpAudioFrameConfig _audioConfig;
};

} // namespace RTC
