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
                  uint16_t sequenceNumber,
                  const std::shared_ptr<const RtpAudioFrameConfig>& audioConfig);
    // overrides of RtpMediaFrame
    std::shared_ptr<const RtpAudioFrameConfig> GetAudioConfig() const final { return _audioConfig; }
private:
    const std::shared_ptr<const RtpAudioFrameConfig> _audioConfig;
};

class RtpMediaFrame::RtpVideoFrame : public RtpMediaFrame
{
public:
    RtpVideoFrame(const RtpCodecMimeType& codecMimeType,
                  const std::shared_ptr<const MemoryBuffer>& payload,
                  bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                  uint16_t sequenceNumber,
                  const std::shared_ptr<const RtpVideoFrameConfig>& videoConfig);
    // overrides of RtpMediaFrame
    std::shared_ptr<const RtpVideoFrameConfig> GetVideoConfig() const { return _videoConfig; }
private:
    const std::shared_ptr<const RtpVideoFrameConfig> _videoConfig;
};

RtpMediaFrame::RtpMediaFrame(const RtpCodecMimeType& mimeType,
                             const std::shared_ptr<const MemoryBuffer>& payload,
                             bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                             uint16_t sequenceNumber)
    : _mimeType(mimeType)
    , _payload(payload)
    , _isKeyFrame(isKeyFrame)
    , _timestamp(timestamp)
    , _ssrc(ssrc)
    , _sequenceNumber(sequenceNumber)
{
    MS_ASSERT(_mimeType.IsMediaCodec(), "invalid media codec");
    MS_ASSERT(_payload && !_payload->IsEmpty(), "wrong payload");
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateAudio(const RtpPacket* packet,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          const std::shared_ptr<const RtpAudioFrameConfig>& audioConfig,
                                                          KeyFrameMark keyFrameMark,
                                                          const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet) {
        const auto payload = CreatePayload(packet, payloadAllocator);
        return CreateAudio(packet, payload, codecType, audioConfig, keyFrameMark);
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateAudio(const RtpPacket* packet,
                                                          const std::shared_ptr<const MemoryBuffer>& payload,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          const std::shared_ptr<const RtpAudioFrameConfig>& audioConfig,
                                                          KeyFrameMark keyFrameMark)
{
    if (packet && payload) {
        const RtpCodecMimeType codecMimeType(RtpCodecMimeType::Type::AUDIO, codecType);
        MS_ASSERT(codecMimeType.IsAudioCodec(), "is not audio codec");
        return std::make_shared<RtpAudioFrame>(codecMimeType, payload,
                                               IsKeyFrame(packet, keyFrameMark),
                                               packet->GetTimestamp(), packet->GetSsrc(),
                                               packet->GetSequenceNumber(), audioConfig);
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateVideo(const RtpPacket* packet,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          const std::shared_ptr<const RtpVideoFrameConfig>& videoConfig,
                                                          KeyFrameMark keyFrameMark,
                                                          const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet) {
        const auto payload = CreatePayload(packet, payloadAllocator);
        return CreateVideo(packet, payload, codecType, videoConfig, keyFrameMark);
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::CreateVideo(const RtpPacket* packet,
                                                          const std::shared_ptr<const MemoryBuffer>& payload,
                                                          RtpCodecMimeType::Subtype codecType,
                                                          const std::shared_ptr<const RtpVideoFrameConfig>& videoConfig,
                                                          KeyFrameMark keyFrameMark)
{
    if (packet && payload) {
        const RtpCodecMimeType codecMimeType(RtpCodecMimeType::Type::VIDEO, codecType);
        MS_ASSERT(codecMimeType.IsVideoCodec(), "is not video codec");
        return std::make_shared<RtpVideoFrame>(codecMimeType, payload,
                                               IsKeyFrame(packet, keyFrameMark),
                                               packet->GetTimestamp(), packet->GetSsrc(),
                                               packet->GetSequenceNumber(), videoConfig);
    }
    return nullptr;
}

std::shared_ptr<const MemoryBuffer> RtpMediaFrame::CreatePayload(const RtpPacket* packet,
                                                                 const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet) {
        return SimpleMemoryBuffer::Create(packet->GetPayload(), packet->GetPayloadLength(), payloadAllocator);
    }
    return nullptr;
}

bool RtpMediaFrame::IsKeyFrame(const RtpPacket* packet, KeyFrameMark keyFrameMark)
{
    switch (keyFrameMark) {
    case ForceOn:
            return true;
        case ForceOff:
            return false;
        default:
            break;
    }
    return packet->IsKeyFrame();
}

RtpMediaFrame::RtpAudioFrame::RtpAudioFrame(const RtpCodecMimeType& codecMimeType,
                                            const std::shared_ptr<const MemoryBuffer>& payload,
                                            bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                                            uint16_t sequenceNumber,
                                            const std::shared_ptr<const RtpAudioFrameConfig>& audioConfig)
    : RtpMediaFrame(codecMimeType, payload, isKeyFrame, timestamp, ssrc, sequenceNumber)
    , _audioConfig(audioConfig)
{
}

RtpMediaFrame::RtpVideoFrame::RtpVideoFrame(const RtpCodecMimeType& codecMimeType,
                                            const std::shared_ptr<const MemoryBuffer>& payload,
                                            bool isKeyFrame, uint32_t timestamp, uint32_t ssrc,
                                            uint16_t sequenceNumber,
                                            const std::shared_ptr<const RtpVideoFrameConfig>& videoConfig)
    : RtpMediaFrame(codecMimeType, payload, isKeyFrame, timestamp, ssrc, sequenceNumber)
    , _videoConfig(videoConfig)
{
}

RtpMediaFrameConfig::~RtpMediaFrameConfig()
{
}

std::shared_ptr<const MemoryBuffer> RtpMediaFrameConfig::GetCodecSpecificData() const
{
    return std::atomic_load(&_codecSpecificData);
}

void RtpMediaFrameConfig::SetCodecSpecificData(const std::shared_ptr<const MemoryBuffer>& codecSpecificData)
{
    std::atomic_store(&_codecSpecificData, codecSpecificData);
}

void RtpAudioFrameConfig::SetChannelCount(uint8_t channelCount)
{
    MS_ASSERT(channelCount, "channels count must be greater than zero");
    _channelCount = channelCount;
}

void RtpAudioFrameConfig::SetBitsPerSample(uint8_t bitsPerSample)
{
    MS_ASSERT(bitsPerSample, "bits per sample must be greater than zero");
    MS_ASSERT(0U == bitsPerSample % 8, "bits per sample must be a multiple of 8");
    _bitsPerSample = bitsPerSample;
}

std::string RtpAudioFrameConfig::ToString() const
{
    return std::to_string(GetChannelCount()) + " channels, " +
           std::to_string(GetBitsPerSample()) + " bits";
}

void RtpVideoFrameConfig::SetWidth(int32_t width)
{
    _width = width;
}

void RtpVideoFrameConfig::SetHeight(int32_t height)
{
    _height = height;
}

void RtpVideoFrameConfig::SetFrameRate(double frameRate)
{
    _frameRate = frameRate;
}

std::string RtpVideoFrameConfig::ToString() const
{
    return std::to_string(GetWidth()) + "x" + std::to_string(GetHeight()) +
           " px, " + std::to_string(GetFrameRate()) + " fps";
}

} // namespace RTC
