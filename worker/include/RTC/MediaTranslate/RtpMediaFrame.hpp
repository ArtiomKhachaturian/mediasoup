#pragma once

#include "RTC/MediaTranslate/MediaFrame.hpp"
#include <optional>

namespace RTC
{

class RtpPacket;

class RtpMediaFrame : public MediaFrame
{
public:
    RtpMediaFrame(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                  const std::weak_ptr<BufferAllocator>& allocator);
    ~RtpMediaFrame() final = default;
    uint32_t GetRtpTimestamp() const { return GetTimestamp().GetRtpTime(); }
    void SetRtpTimestamp(uint32_t rtpTimestamp) { SetTimestamp(rtpTimestamp); }
    // packets management
    // add-methods return false if input arguments is incorrect: null or empty packet/payload
    bool AddPacket(const RtpPacket* packet);
    bool AddPacket(const RtpPacket* packet, const uint8_t* data, size_t len);

    // factory methods (for single-packet frames)
    static std::shared_ptr<RtpMediaFrame> Create(const RtpPacket* packet,
                                                 const RtpCodecMimeType& mimeType, uint32_t clockRate,
                                                 const std::weak_ptr<BufferAllocator>& allocator);
    // null_opt if no descriptor for the packet
    static std::optional<size_t> GetPayloadDescriptorSize(const RtpPacket* packet);
    static bool ParseVp8VideoConfig(const RtpPacket* packet, const std::shared_ptr<VideoFrameConfig>& applyTo);
    static bool ParseVp9VideoConfig(const RtpPacket* packet, const std::shared_ptr<VideoFrameConfig>& applyTo);
};

} // namespace RTC
