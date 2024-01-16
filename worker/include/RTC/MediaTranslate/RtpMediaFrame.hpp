#pragma once

#include "RTC/MediaTranslate/RtpMediaPacketInfo.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include <optional>

namespace RTC
{

class RtpPacket;

class RtpMediaFrame : public MediaFrame
{
public:
    RtpMediaFrame(bool audio, RtpCodecMimeType::Subtype codecType);
    RtpMediaFrame(const RtpCodecMimeType& mimeType);
    ~RtpMediaFrame() final;
    // packets management
    // add-methods return false if input arguments is incorrect: null or empty packet/payload
    bool AddPacket(const RtpPacket* packet, std::vector<uint8_t> payload);
    bool AddPacket(const RtpPacket* packet, const uint8_t* data, size_t len,
                   const std::allocator<uint8_t>& payloadAllocator = {});
    bool AddPacket(const RtpPacket* packet, const std::allocator<uint8_t>& payloadAllocator = {});
    bool AddPacket(const RtpPacket* packet, const std::shared_ptr<const MemoryBuffer>& payload);
    const std::vector<RtpMediaPacketInfo>& GetPacketsInfo() const;
    uint32_t GetTimestamp() const { return _timestamp; }
    uint32_t GetSsrc() const { return _ssrc; }
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
    // null_opt if no descriptor for the packet
    static std::optional<size_t> GetPayloadDescriptorSize(const RtpPacket* packet);
    static bool ParseVp8VideoConfig(const RtpPacket* packet, const std::shared_ptr<VideoFrameConfig>& applyTo);
    static bool ParseVp9VideoConfig(const RtpPacket* packet, const std::shared_ptr<VideoFrameConfig>& applyTo);
private:
    uint32_t _ssrc = 0U;
    uint32_t _timestamp = 0U;
    std::vector<RtpMediaPacketInfo> _packetsInfo;
};

} // namespace RTC
