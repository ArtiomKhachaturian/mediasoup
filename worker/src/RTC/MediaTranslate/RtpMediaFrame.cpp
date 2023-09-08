#define MS_CLASS "RTC::RtpMediaFrame"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpMediaFrame::RtpMediaFrame(bool audio, RtpCodecMimeType::Subtype codecType)
    : RtpMediaFrame(RtpCodecMimeType(audio ? RtpCodecMimeType::Type::AUDIO :
                                     RtpCodecMimeType::Type::VIDEO, codecType))
{
}

RtpMediaFrame::RtpMediaFrame(const RtpCodecMimeType& mimeType)
    : _mimeType(mimeType)
{
    MS_ASSERT(_mimeType.IsMediaCodec(), "invalid media codec");
    _packetsInfo.reserve(_mimeType.IsAudioCodec() ? 1UL : 4UL);
    _packets.reserve(_mimeType.IsAudioCodec() ? 1UL : 4UL);
}

RtpMediaFrame::~RtpMediaFrame()
{
}

bool RtpMediaFrame::AddPacket(const RtpPacket* packet, std::vector<uint8_t> payload)
{
    return AddPacket(packet, SimpleMemoryBuffer::Create(std::move(payload)));
}

bool RtpMediaFrame::AddPacket(const RtpPacket* packet, const uint8_t* data, size_t len,
                              const std::allocator<uint8_t>& payloadAllocator)
{
    return AddPacket(packet, SimpleMemoryBuffer::Create(data, len, payloadAllocator));
}

bool RtpMediaFrame::AddPacket(const RtpPacket* packet,
                              const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet) {
        return AddPacket(packet, packet->GetPayload(), packet->GetPayloadLength(), payloadAllocator);
    }
    return false;
}

bool RtpMediaFrame::AddPacket(const RtpPacket* packet,
                              const std::shared_ptr<const MemoryBuffer>& payload)
{
    if (packet && payload) {
        if (_packetsInfo.empty()) {
            _ssrc = packet->GetSsrc();
        }
        else {
            MS_ASSERT(_ssrc == packet->GetSsrc(), "packet from different media source");
        }
        RtpMediaPacketInfo packetInfo;
        packetInfo._ssrc = packet->GetSsrc();
        packetInfo._sequenceNumber = packet->GetSequenceNumber();
        _packetsInfo.push_back(std::move(packetInfo));
        _packets.push_back(payload);
        _timestamp = packet->GetTimestamp();
        return true;
    }
    return false;
}

const std::vector<RtpMediaPacketInfo>& RtpMediaFrame::GetPacketsInfo() const
{
    return _packetsInfo;
}

const std::vector<std::shared_ptr<const MemoryBuffer>> RtpMediaFrame::GetPackets() const
{
    return _packets;
}

void RtpMediaFrame::SetAudioConfig(const std::shared_ptr<const RtpAudioFrameConfig>& config)
{
    MS_ASSERT(IsAudio(), "set incorrect config for audio track");
    _config = config;
}

std::shared_ptr<const RtpAudioFrameConfig> RtpMediaFrame::GetAudioConfig() const
{
    return std::dynamic_pointer_cast<const RtpAudioFrameConfig>(_config);
}

void RtpMediaFrame::SetVideoConfig(const std::shared_ptr<const RtpVideoFrameConfig>& config)
{
    MS_ASSERT(!IsAudio(), "set incorrect config for video track");
    _config = config;
}

std::shared_ptr<const RtpVideoFrameConfig> RtpMediaFrame::GetVideoConfig() const
{
    return std::dynamic_pointer_cast<const RtpVideoFrameConfig>(_config);
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::Create(const RtpCodecMimeType& mimeType,
                                                     const RtpPacket* packet, std::vector<uint8_t> payload)
{
    if (packet && mimeType && !payload.empty()) {
        auto frame = std::make_shared<RtpMediaFrame>(mimeType);
        if (frame->AddPacket(packet, std::move(payload))) {
            return frame;
        }
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::Create(const RtpCodecMimeType& mimeType,
                                                     const RtpPacket* packet,
                                                     const uint8_t* data, size_t len,
                                                     const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet && data && len && mimeType) {
        auto frame = std::make_shared<RtpMediaFrame>(mimeType);
        if (frame->AddPacket(packet, data, len, payloadAllocator)) {
            return frame;
        }
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::Create(const RtpCodecMimeType& mimeType,
                                                     const RtpPacket* packet,
                                                     const std::allocator<uint8_t>& payloadAllocator)
{
    if (packet && mimeType) {
        auto frame = std::make_shared<RtpMediaFrame>(mimeType);
        if (frame->AddPacket(packet, payloadAllocator)) {
            return frame;
        }
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::Create(const RtpCodecMimeType& mimeType,
                                                     const RtpPacket* packet,
                                                     const std::shared_ptr<const MemoryBuffer>& payload)
{
    if (packet && payload && mimeType) {
        auto frame = std::make_shared<RtpMediaFrame>(mimeType);
        if (frame->AddPacket(packet, payload)) {
            return frame;
        }
    }
    return nullptr;
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
