#define MS_CLASS "RTC::RtpMediaFrame"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

class RtpMediaFrame::RtpAudioFrame : public RtpMediaFrame
{
public:
    RtpAudioFrame(const RtpCodecMimeType& codecMimeType,
                  const std::shared_ptr<const MemoryBuffer>& payload,
                  bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                  uint16_t sequenceNumber, uint32_t sampleRate,
                  RtpAudioFrameConfig audioConfig);
    // overrides of RtpMediaFrame
    const RtpAudioFrameConfig* GetAudioConfig() const final { return &_audioConfig; }
private:
    const RtpAudioFrameConfig _audioConfig;
};

class RtpMediaFrame::RtpVideoFrame : public RtpMediaFrame
{
public:
    RtpVideoFrame(const RtpCodecMimeType& codecMimeType,
                  const std::shared_ptr<const MemoryBuffer>& payload,
                  bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                  uint16_t sequenceNumber, uint32_t sampleRate,
                  RtpVideoFrameConfig videoConfig);
    // overrides of RtpMediaFrame
    const RtpVideoFrameConfig* GetVideoConfig() const { return &_videoConfig; }
private:
    const RtpVideoFrameConfig _videoConfig;
};

RtpMediaFrame::RtpMediaFrame(const RtpCodecMimeType& codecMimeType,
                             const std::shared_ptr<const MemoryBuffer>& payload,
                             bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                             uint16_t sequenceNumber, uint32_t sampleRate)
    : _codecMimeType(codecMimeType)
    , _payload(payload)
    , _isKeyFrame(isKeyFrame)
    , _timestamp(timestamp)
    , _ssrc(ssrc)
    , _sequenceNumber(sequenceNumber)
    , _sampleRate(sampleRate)
{
    MS_ASSERT(_codecMimeType.IsMediaCodec(), "invalid media codec");
    MS_ASSERT(_payload && !_payload->IsEmpty(), "wrong payload");
    MS_ASSERT(_sampleRate, "sample rate must be greater than zero");
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateAudio(const RtpPacket* packet,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          uint32_t sampleRate,
                                                          RtpAudioFrameConfig audioConfig,
                                                          const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet) {
        const auto payload = CreatePayload(packet, payloadAllocator);
        return CreateAudio(packet, payload, codecType, sampleRate, std::move(audioConfig));
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateAudio(const RtpPacket* packet,
                                                          const std::shared_ptr<const MemoryBuffer>& payload,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          uint32_t sampleRate,
                                                          RtpAudioFrameConfig audioConfig)
{
    if (packet && payload) {
        const RtpCodecMimeType codecMimeType(RtpCodecMimeType::Type::AUDIO, codecType);
        MS_ASSERT(codecMimeType.IsAudioCodec(), "is not audio codec");
        return std::make_shared<RtpAudioFrame>(codecMimeType, payload, packet->IsKeyFrame(),
                                               packet->GetTimestamp(), packet->GetSsrc(),
                                               packet->GetSequenceNumber(), sampleRate,
                                               std::move(audioConfig));
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateVideo(const RtpPacket* packet,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          uint32_t sampleRate,
                                                          RtpVideoFrameConfig videoConfig,
                                                          const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet) {
        const auto payload = CreatePayload(packet, payloadAllocator);
        return CreateVideo(packet, payload, codecType, sampleRate, std::move(videoConfig));
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateVideo(const RtpPacket* packet,
                                                          const std::shared_ptr<const MemoryBuffer>& payload,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          uint32_t sampleRate,
                                                          RtpVideoFrameConfig videoConfig)
{
    if (packet && payload) {
        const RtpCodecMimeType codecMimeType(RtpCodecMimeType::Type::VIDEO, codecType);
        MS_ASSERT(codecMimeType.IsVideoCodec(), "is not video codec");
        return std::make_shared<RtpVideoFrame>(codecMimeType, payload, packet->IsKeyFrame(),
                                               packet->GetTimestamp(), packet->GetSsrc(),
                                               packet->GetSequenceNumber(), sampleRate,
                                               std::move(videoConfig));
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
        return SimpleMemoryBuffer::Create(packet->GetPayload(), packet->GetPayloadLength(), payloadAllocator);
    }
    return nullptr;
}

RtpMediaFrame::RtpAudioFrame::RtpAudioFrame(const RtpCodecMimeType& codecMimeType,
                                            const std::shared_ptr<const MemoryBuffer>& payload,
                                            bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                                            uint16_t sequenceNumber, uint32_t sampleRate,
                                            RtpAudioFrameConfig audioConfig)
    : RtpMediaFrame(codecMimeType, payload, isKeyFrame, timestamp, ssrc, sequenceNumber, sampleRate)
    , _audioConfig(std::move(audioConfig))
{
    MS_ASSERT(_audioConfig._channelCount, "channels count must be greater than zero");
    MS_ASSERT(_audioConfig._bitsPerSample && (0U == _audioConfig._bitsPerSample % 8), "invalid bits per sample");
}

RtpMediaFrame::RtpVideoFrame::RtpVideoFrame(const RtpCodecMimeType& codecMimeType,
                                            const std::shared_ptr<const MemoryBuffer>& payload,
                                            bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                                            uint16_t sequenceNumber, uint32_t sampleRate,
                                            RtpVideoFrameConfig videoConfig)
    : RtpMediaFrame(codecMimeType, payload, isKeyFrame, timestamp, ssrc, sequenceNumber, sampleRate)
    , _videoConfig(std::move(videoConfig))
{
    if (isKeyFrame) {
        MS_ASSERT(_videoConfig._height > 0, "video height should be greater than zero");
        MS_ASSERT(_videoConfig._width > 0, "video width should be greater than zero");
    }
}

} // namespace RTC
