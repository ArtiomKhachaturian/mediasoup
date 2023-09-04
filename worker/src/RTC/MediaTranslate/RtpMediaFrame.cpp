#define MS_CLASS "RTC::RtpMediaFrame"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpMediaFrame::RtpMediaFrame(const RtpCodecMimeType& codecMimeType, bool isKeyFrame,
                             std::vector<uint8_t> payload, uint32_t timestamp, uint32_t ssrc,
                             uint16_t sequenceNumber, uint32_t durationMs,
                             std::unique_ptr<RtpMediaConfig> mediaConfig)
    : _codecMimeType(codecMimeType)
    , _isKeyFrame(isKeyFrame)
    , _payload(std::move(payload))
    , _timestamp(timestamp)
    , _ssrc(ssrc)
    , _sequenceNumber(sequenceNumber)
    , _durationMs(durationMs)
    , _mediaConfig(std::move(mediaConfig))
{
    MS_ASSERT(_durationMs > 0U, "invalid media frame duration");
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::create(const RtpPacket* packet,
                                                     const RtpCodecMimeType& codecMimeType,
                                                     uint32_t durationMs,
                                                     std::unique_ptr<RtpMediaConfig> mediaConfig,
                                                     const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet && mediaConfig && IsValidMime(codecMimeType)) {
        auto payload = CreatePayloadCopy(packet, payloadAllocator);
        if (!payload.empty()) {
            return std::make_shared<RtpMediaFrame>(codecMimeType, packet->IsKeyFrame(),
                                                   std::move(payload),
                                                   packet->GetTimestamp(), packet->GetSsrc(),
                                                   packet->GetSequenceNumber(), durationMs,
                                                   std::move(mediaConfig));
        }
    }
    return nullptr;
}

bool RtpMediaFrame::IsValidMime(const RtpCodecMimeType& mime)
{
    return mime.IsMediaCodec() && (RtpCodecMimeType::Type::AUDIO == mime.type ||
                                   RtpCodecMimeType::Type::VIDEO == mime.type);
}

std::vector<uint8_t> RtpMediaFrame::CreatePayloadCopy(const RtpPacket* packet,
                                                      const std::allocator<uint8_t>& payloadAllocator)

{
    if (packet) {
        const auto payload = packet->GetPayload();
        const auto payloadLen = packet->GetPayloadLength();
        if (payload && payloadLen) {
            return std::vector<uint8_t>(payload, payload + payloadLen, payloadAllocator);
        }
    }
    return {};
}

const RtpAudioConfig* RtpMediaFrame::GetAudioConfig() const
{
    switch (GetCodecMimeType().type) {
        case RtpCodecMimeType::Type::AUDIO:
            return static_cast<const RtpAudioConfig*>(_mediaConfig.get());
        default:
            MS_ASSERT(false, "audio config is not available");
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
            MS_ASSERT(false, "video config is not available");
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
