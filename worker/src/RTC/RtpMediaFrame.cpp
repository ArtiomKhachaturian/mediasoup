#define MS_CLASS "RTC::RtpMediaFrame"
#include "RTC/RtpMediaFrame.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpMediaFrame::RtpMediaFrame(const RtpCodecMimeType& codecMimeType,
                             std::vector<uint8_t> payload,
                             uint32_t timestamp, uint32_t ssrc,
                             uint16_t sequenceNumber,
                             uint32_t duration,
                             std::unique_ptr<RtpMediaConfig> mediaConfig)
    : _codecMimeType(codecMimeType)
    , _payload(std::move(payload))
    , _timestamp(timestamp)
    , _ssrc(ssrc)
    , _sequenceNumber(sequenceNumber)
    , _duration(duration)
    , _mediaConfig(std::move(mediaConfig))
{
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::create(const RtpPacket* packet,
                                                     const RtpCodecMimeType& codecMimeType,
                                                     const std::allocator<uint8_t>& payloadAllocator,
                                                     uint32_t duration,
                                                     std::unique_ptr<RtpMediaConfig> mediaConfig)
{
    if (packet) {
        auto payload = CreatePayloadCopy(packet, payloadAllocator);
        if (!payload.empty()) {
            return std::make_shared<RtpMediaFrame>(codecMimeType, std::move(payload),
                                                   packet->GetTimestamp(), packet->GetSsrc(),
                                                   packet->GetSequenceNumber(), duration,
                                                   std::move(mediaConfig));
        }
    }
    return nullptr;
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

const RtpAudioConfig* RtpMediaFrame::audioConfig() const
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

const RtpVideoConfig* RtpMediaFrame::videoConfig() const
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

RtpAudioConfig::RtpAudioConfig(uint8_t channelCount, uint32_t sampleRate)
    : _channelCount(channelCount)
    , _sampleRate(sampleRate)
{
}

} // namespace RTC
