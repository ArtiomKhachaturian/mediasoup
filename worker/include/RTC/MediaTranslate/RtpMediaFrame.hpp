#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/RtpMediaPacketInfo.hpp"
#include "MemoryBuffer.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class RtpPacket;
class RtpMediaFrameConfig;
class RtpAudioFrameConfig;
class RtpVideoFrameConfig;

class RtpMediaFrame
{
public:
    RtpMediaFrame(bool audio, RtpCodecMimeType::Subtype codecType);
    RtpMediaFrame(const RtpCodecMimeType& mimeType);
    ~RtpMediaFrame();
    // packets management
    // add-methods return false if input arguments is incorrect: null or empty packet/payload
    bool AddPacket(const RtpPacket* packet, std::vector<uint8_t> payload);
    bool AddPacket(const RtpPacket* packet, const uint8_t* data, size_t len,
                   const std::allocator<uint8_t>& payloadAllocator = {});
    bool AddPacket(const RtpPacket* packet, const std::allocator<uint8_t>& payloadAllocator = {});
    bool AddPacket(const RtpPacket* packet, const std::shared_ptr<const MemoryBuffer>& payload);
    bool IsEmpty() const { return _packetsInfo.empty(); }
    const std::vector<RtpMediaPacketInfo>& GetPacketsInfo() const;
    const std::vector<std::shared_ptr<const MemoryBuffer>> GetPackets() const;
    // expensive operation, in difference from [GetPackets] returns continuous area of payload data
    std::shared_ptr<const MemoryBuffer> GetPayload() const;
    size_t GetPayloadSize() const { return _payloadSize; }
    // common properties
    const RtpCodecMimeType& GetMimeType() const { return _mimeType; }
    bool IsAudio() const { return GetMimeType().IsAudioCodec(); }
    bool IsKeyFrame() const { return _isKeyFrame; }
    uint32_t GetTimestamp() const { return _timestamp; }
    uint32_t GetSsrc() const { return _ssrc; }
    // audio configuration
    void SetAudioConfig(const std::shared_ptr<const RtpAudioFrameConfig>& config);
    std::shared_ptr<const RtpAudioFrameConfig> GetAudioConfig() const;
    // video configuration
    void SetVideoConfig(const std::shared_ptr<const RtpVideoFrameConfig>& config);
    std::shared_ptr<const RtpVideoFrameConfig> GetVideoConfig() const;
    // factory methods (for single-packet frames)
    static std::shared_ptr<RtpMediaFrame> Create(const RtpCodecMimeType& mimeType,
                                                 const RtpPacket* packet,
                                                 std::vector<uint8_t> payload);
    static std::shared_ptr<RtpMediaFrame> Create(const RtpCodecMimeType& mimeType,
                                                 const RtpPacket* packet,
                                                 const uint8_t* data, size_t len,
                                                 const std::allocator<uint8_t>& payloadAllocator = {});
    static std::shared_ptr<RtpMediaFrame> Create(const RtpCodecMimeType& mimeType,
                                                 const RtpPacket* packet,
                                                 const std::allocator<uint8_t>& payloadAllocator = {});
    static std::shared_ptr<RtpMediaFrame> Create(const RtpCodecMimeType& mimeType,
                                                 const RtpPacket* packet,
                                                 const std::shared_ptr<const MemoryBuffer>& payload);
private:
    const RtpCodecMimeType _mimeType;
    bool _isKeyFrame = false;
    uint32_t _ssrc = 0U;
    uint32_t _timestamp = 0U;
    size_t _payloadSize = 0UL;
    std::vector<RtpMediaPacketInfo> _packetsInfo;
    std::vector<std::shared_ptr<const MemoryBuffer>> _packets;
    std::shared_ptr<const RtpMediaFrameConfig> _config;
};

} // namespace RTC
