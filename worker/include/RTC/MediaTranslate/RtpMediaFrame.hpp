#pragma once

#include "RTC/MediaTranslate/MediaFrame.hpp"
#include <optional>

namespace RTC
{

class RtpPacket;

class RtpMediaFrame : public MediaFrame
{
public:
    RtpMediaFrame(bool audio, RtpCodecMimeType::Subtype codecType, uint32_t clockRate);
    RtpMediaFrame(const RtpCodecMimeType& mimeType, uint32_t clockRate);
    ~RtpMediaFrame() final;
    uint32_t GetRtpTimestamp() const { return GetTimestamp().GetRtpTime(); }
    void SetRtpTimestamp(uint32_t rtpTimestamp) { SetTimestamp(rtpTimestamp); }
    // packets management
    // add-methods return false if input arguments is incorrect: null or empty packet/payload
    bool AddPacket(const RtpPacket* packet, std::vector<uint8_t> payload);
    bool AddPacket(const RtpPacket* packet, const uint8_t* data, size_t len,
                   const std::allocator<uint8_t>& payloadAllocator = {});
    bool AddPacket(const RtpPacket* packet, const std::allocator<uint8_t>& payloadAllocator = {});
    bool AddPacket(const RtpPacket* packet, const std::shared_ptr<MemoryBuffer>& payload);
    // factory methods (for single-packet frames)
    static std::shared_ptr<RtpMediaFrame> Create(const RtpCodecMimeType& mimeType,
                                                 uint32_t clockRate, const RtpPacket* packet,
                                                 std::vector<uint8_t> payload);
    static std::shared_ptr<RtpMediaFrame> Create(const RtpCodecMimeType& mimeType,
                                                 uint32_t clockRate, const RtpPacket* packet,
                                                 const uint8_t* data, size_t len,
                                                 const std::allocator<uint8_t>& payloadAllocator = {});
    static std::shared_ptr<RtpMediaFrame> Create(const RtpCodecMimeType& mimeType,
                                                 uint32_t clockRate, const RtpPacket* packet,
                                                 const std::allocator<uint8_t>& payloadAllocator = {});
    static std::shared_ptr<RtpMediaFrame> Create(const RtpCodecMimeType& mimeType,
                                                 uint32_t clockRate, const RtpPacket* packet,
                                                 const std::shared_ptr<MemoryBuffer>& payload);
    // null_opt if no descriptor for the packet
    static std::optional<size_t> GetPayloadDescriptorSize(const RtpPacket* packet);
    static bool ParseVp8VideoConfig(const RtpPacket* packet, const std::shared_ptr<VideoFrameConfig>& applyTo);
    static bool ParseVp9VideoConfig(const RtpPacket* packet, const std::shared_ptr<VideoFrameConfig>& applyTo);
};

} // namespace RTC
