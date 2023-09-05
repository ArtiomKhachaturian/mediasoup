#pragma once

#include "RTC/MediaTranslate/RtpMediaFrame.hpp"

namespace RTC
{

class RtpVideoFrame : public RtpMediaFrame
{
public:
    RtpVideoFrame(const RtpCodecMimeType& codecMimeType,
                  const std::shared_ptr<const MemoryBuffer>& payload,
                  bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                  uint16_t sequenceNumber, uint32_t sampleRate,
                  const RtpVideoFrameConfig& videoConfig,
                  uint32_t durationMs = 0U);
    // overrides of RtpMediaFrame
    const RtpVideoFrameConfig* GetVideoConfig() const { return &_videoConfig; }
private:
    const RtpVideoFrameConfig _videoConfig;
};

} // namespace RTC
