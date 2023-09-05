#define MS_CLASS "RTC::RtpVideoFrame"
#include "RTC/MediaTranslate/RtpVideoFrame.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpVideoFrame::RtpVideoFrame(const RtpCodecMimeType& codecMimeType,
                             const std::shared_ptr<const MemoryBuffer>& payload,
                             bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                             uint16_t sequenceNumber, uint32_t sampleRate,
                             const RtpVideoFrameConfig& videoConfig,
                             uint32_t durationMs)
    : RtpMediaFrame(codecMimeType, payload, isKeyFrame, timestamp,
                    ssrc, sequenceNumber, sampleRate, durationMs)
    , _videoConfig(videoConfig)
{
    MS_ASSERT(codecMimeType.IsVideoCodec(), "mime type is not video");
    if (isKeyFrame) {
        MS_ASSERT(_videoConfig._height > 0, "video height should be greater than zero");
        MS_ASSERT(_videoConfig._width > 0, "video width should be greater than zero");
    }
}

} // namespace RTC
