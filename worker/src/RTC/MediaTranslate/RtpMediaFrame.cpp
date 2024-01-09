#define MS_CLASS "RTC::RtpMediaFrame"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/RtpAudioFrameConfig.hpp"
#include "RTC/MediaTranslate/RtpVideoFrameConfig.hpp"
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
    if (packet && payload && !payload->IsEmpty()) {
        if (_packetsInfo.empty()) {
            _ssrc = packet->GetSsrc();
        }
        else {
            MS_ASSERT(_ssrc == packet->GetSsrc(), "packet from different media source");
        }
        RtpMediaPacketInfo packetInfo;
        packetInfo._sequenceNumber = packet->GetSequenceNumber();
        _packetsInfo.push_back(std::move(packetInfo));
        _packets.push_back(payload);
        if (_timestamp > packet->GetTimestamp()) {
            MS_WARN_DEV("time stamp of new packet is less than previous, SSRC = %du", _ssrc);
        }
        else {
            _timestamp = packet->GetTimestamp();
        }
        if (packet->IsKeyFrame()) {
            _isKeyFrame = true;
        }
        _payloadSize += payload->GetSize();
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

std::shared_ptr<const MemoryBuffer> RtpMediaFrame::GetPayload() const
{
    if (!_packets.empty()) {
        if (_packets.size() > 1UL) { // merge all packets into continious area
            const auto payload = std::make_shared<SimpleMemoryBuffer>();
            payload->Reserve(GetPayloadSize());
            for (const auto& packet : _packets) {
                payload->Append(*packet);
            }
            if (!payload->IsEmpty()) {
                return payload;
            }
        }
        return _packets.front();
    }
    return nullptr;
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
    if (packet && !payload.empty()) {
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
    if (packet && data && len) {
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
    if (packet) {
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
    if (packet && payload) {
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

std::optional<size_t> RtpMediaFrameConfig::GetPayloadDescriptorSize(const RtpPacket* packet)
{
    if (packet) {
        if (const auto pdh = packet->GetPayloadDescriptorHandler()) {
            return pdh->GetPayloadDescriptorSize();
        }
    }
    return std::nullopt;
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

bool RtpVideoFrameConfig::ParseVp8VideoConfig(const RtpPacket* packet)
{
    if (const auto pds = GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            const auto offset = pds.value();
            const auto len = packet->GetPayloadLength();
            if (len >= offset + 10U) {
                // Start code for VP8 key frame:
                // Read comon 3 bytes
                //   0 1 2 3 4 5 6 7
                //  +-+-+-+-+-+-+-+-+
                //  |Size0|H| VER |P|
                //  +-+-+-+-+-+-+-+-+
                //  |     Size1     |
                //  +-+-+-+-+-+-+-+-+
                //  |     Size2     |
                //  +-+-+-+-+-+-+-+-+
                // Keyframe header consists of a three-byte sync code
                // followed by the width and height and associated scaling factors
                if (payload[offset + 3U] == 0x9d &&
                    payload[offset + 4U] == 0x01 &&
                    payload[offset + 5U] == 0x2a) {
                    const uint16_t hor = payload[offset + 7U] << 8 | payload[offset + 6U];
                    const uint16_t ver = payload[offset + 9U] << 8 | payload[offset + 8U];
                    SetWidth(hor & 0x3fff);
                    //_videoConfig._widthScale = hor >> 14;
                    SetHeight(ver & 0x3fff);
                    //_videoConfig._heightScale = ver >> 14;
                    return true;
                }
            }
        }
    }
    return false;
}

bool RtpVideoFrameConfig::ParseVp9VideoConfig(const RtpPacket* packet)
{
    if (const auto pds = GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            //const auto offset = pds.value();
            /*vp9_parser::Vp9HeaderParser parser;
             if (parser.ParseUncompressedHeader(payload, packet->GetPayloadLength())) {
             videoConfig._width = parser.width();
             videoConfig._height = parser.height();
             return true;
             }*/
        }
    }
    return false;
}

std::string RtpVideoFrameConfig::ToString() const
{
    return std::to_string(GetWidth()) + "x" + std::to_string(GetHeight()) +
           " px, " + std::to_string(GetFrameRate()) + " fps";
}

} // namespace RTC
