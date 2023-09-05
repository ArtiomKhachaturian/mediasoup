#define MS_CLASS "RTC::RtpMediaFrame"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpMediaFrame::RtpMediaFrame(const RtpCodecMimeType& codecMimeType,
                             const std::shared_ptr<const MemoryBuffer>& payload,
                             bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                             uint16_t sequenceNumber, uint32_t durationMs,
                             std::unique_ptr<RtpMediaConfig> mediaConfig)
    : _codecMimeType(codecMimeType)
    , _payload(payload)
    , _isKeyFrame(isKeyFrame)
    , _timestamp(timestamp)
    , _ssrc(ssrc)
    , _sequenceNumber(sequenceNumber)
    , _durationMs(durationMs)
    , _mediaConfig(std::move(mediaConfig))
{
    MS_ASSERT(_payload, "payload must not be null");
    MS_ASSERT(_durationMs > 0U, "invalid media frame duration");
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::create(const RtpPacket* packet,
                                                     const RtpCodecMimeType& codecMimeType,
                                                     uint32_t durationMs,
                                                     std::unique_ptr<RtpMediaConfig> mediaConfig,
                                                     const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet && IsValidMediaMime(codecMimeType)) {
        const auto payload = SimpleMemoryBuffer::Create(packet->GetPayload(),
                                                        packet->GetPayloadLength(),
                                                        payloadAllocator);
        if (payload) {
            return std::make_shared<RtpMediaFrame>(codecMimeType, payload, packet->IsKeyFrame(),
                                                   packet->GetTimestamp(), packet->GetSsrc(),
                                                   packet->GetSequenceNumber(), durationMs,
                                                   std::move(mediaConfig));
        }
    }
    return nullptr;
}

bool RtpMediaFrame::IsAudio() const
{
    return IsAudioMime(_codecMimeType);
}

const RtpAudioConfig* RtpMediaFrame::GetAudioConfig() const
{
    switch (GetCodecMimeType().type) {
        case RtpCodecMimeType::Type::AUDIO:
            return static_cast<const RtpAudioConfig*>(_mediaConfig.get());
        default:
            break;
    }
    return nullptr;
}

const RtpVideoConfig* RtpMediaFrame::GetVideoConfig() const
{
    switch (GetCodecMimeType().type) {
        case RtpCodecMimeType::Type::VIDEO:
            return static_cast<const RtpVideoConfig*>(_mediaConfig.get());
        default:
            break;
    }
    return nullptr;
}

RtpAudioConfig::RtpAudioConfig(uint8_t channelCount, uint32_t sampleRate, uint8_t bitsPerSample)
    : _channelCount(channelCount)
    , _sampleRate(sampleRate)
    , _bitsPerSample(bitsPerSample)
{
}

RtpVideoConfig::RtpVideoConfig(int32_t width, int32_t height, double frameRate)
    : _width(width)
    , _height(height)
    , _frameRate(frameRate)
{
}

} // namespace RTC
