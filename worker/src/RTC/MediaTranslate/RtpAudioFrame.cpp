#define MS_CLASS "RTC::RtpAudioFrame"
#include "RTC/MediaTranslate/RtpAudioFrame.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpAudioFrame::RtpAudioFrame(const RtpCodecMimeType& codecMimeType,
                             const std::shared_ptr<const MemoryBuffer>& payload,
                             bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                             uint16_t sequenceNumber, uint32_t sampleRate,
                             const RtpAudioFrameConfig& audioConfig,
                             uint32_t durationMs)
    : RtpMediaFrame(codecMimeType, payload, isKeyFrame, timestamp, ssrc,
                    sequenceNumber, sampleRate, durationMs)
    , _audioConfig(audioConfig)
{
    MS_ASSERT(codecMimeType.IsAudioCodec(), "mime type is not audio");
    MS_ASSERT(_audioConfig._channelCount, "channels count must be greater than zero");
    MS_ASSERT(_audioConfig._bitsPerSample && (0U == _audioConfig._bitsPerSample % 8), "invalid bits per sample");
}

} // namespace RTC
