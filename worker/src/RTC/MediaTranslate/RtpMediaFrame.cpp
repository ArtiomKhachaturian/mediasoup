#define MS_CLASS "RTC::RtpMediaFrame"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/RtpAudioFrame.hpp"
#include "RTC/MediaTranslate/RtpVideoFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpMediaFrame::RtpMediaFrame(const RtpCodecMimeType& codecMimeType,
                             const std::shared_ptr<const MemoryBuffer>& payload,
                             bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                             uint16_t sequenceNumber, uint32_t sampleRate, uint32_t durationMs)
    : _codecMimeType(codecMimeType)
    , _payload(payload)
    , _isKeyFrame(isKeyFrame)
    , _timestamp(timestamp)
    , _ssrc(ssrc)
    , _sequenceNumber(sequenceNumber)
    , _sampleRate(sampleRate)
    , _durationMs(durationMs)
{
    MS_ASSERT(_codecMimeType.IsMediaCodec(), "invalid media codec");
    MS_ASSERT(_payload && !_payload->IsEmpty(), "wrong payload");
    MS_ASSERT(_sampleRate, "sample rate must be greater than zero");
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateAudio(const RtpPacket* packet,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          uint32_t sampleRate,
                                                          const RtpAudioFrameConfig& audioConfig,
                                                          uint32_t durationMs,
                                                          const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet) {
        const auto payload = CreatePayload(packet, payloadAllocator);
        return CreateAudio(packet, payload, codecType, sampleRate, audioConfig, durationMs);
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateAudio(const RtpPacket* packet,
                                                          const std::shared_ptr<const MemoryBuffer>& payload,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          uint32_t sampleRate,
                                                          const RtpAudioFrameConfig& audioConfig,
                                                          uint32_t durationMs)
{
    if (packet && payload) {
        const RtpCodecMimeType codecMimeType(RtpCodecMimeType::Type::AUDIO, codecType);
        MS_ASSERT(codecMimeType.IsAudioCodec(), "is not audio codec");
        return std::make_shared<RtpAudioFrame>(codecMimeType, payload, packet->IsKeyFrame(),
                                               packet->GetTimestamp(), packet->GetSsrc(),
                                               packet->GetSequenceNumber(), sampleRate,
                                               audioConfig, durationMs);
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateVideo(const RtpPacket* packet,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          uint32_t sampleRate,
                                                          const RtpVideoFrameConfig& videoConfig,
                                                          uint32_t durationMs,
                                                          const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet) {
        const auto payload = CreatePayload(packet, payloadAllocator);
        return CreateVideo(packet, payload, codecType, sampleRate, videoConfig, durationMs);
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateVideo(const RtpPacket* packet,
                                                          const std::shared_ptr<const MemoryBuffer>& payload,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          uint32_t sampleRate,
                                                          const RtpVideoFrameConfig& videoConfig,
                                                          uint32_t durationMs)
{
    if (packet && payload) {
        const RtpCodecMimeType codecMimeType(RtpCodecMimeType::Type::VIDEO, codecType);
        MS_ASSERT(codecMimeType.IsVideoCodec(), "is not video codec");
        return std::make_shared<RtpVideoFrame>(codecMimeType, payload, packet->IsKeyFrame(),
                                               packet->GetTimestamp(), packet->GetSsrc(),
                                               packet->GetSequenceNumber(), sampleRate,
                                               videoConfig, durationMs);
    }
    return nullptr;
}

bool RtpMediaFrame::IsAudio() const
{
    return _codecMimeType.IsAudioCodec();
}

std::shared_ptr<const MemoryBuffer> RtpMediaFrame::CreatePayload(const RtpPacket* packet,
                                                                 const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet) {
        return SimpleMemoryBuffer::Create(packet->GetPayload(), packet->GetPayloadLength(),
                                          payloadAllocator);
    }
    return nullptr;
}

} // namespace RTC
