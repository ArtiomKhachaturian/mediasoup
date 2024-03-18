#pragma once
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include <unordered_map>

namespace RTC
{

class RtpDepacketizerVpx : public RtpVideoDepacketizer
{
    class RtpAssembly;
    class ShiftedPayloadBuffer;
public:
    RtpDepacketizerVpx(uint32_t clockRate, bool vp8,
                       const std::shared_ptr<BufferAllocator>& allocator);
    ~RtpDepacketizerVpx() final;
    // impl. of RtpDepacketizer
    MediaFrame FromRtpPacket(uint32_t ssrc, uint32_t rtpTimestamp,
                             bool keyFrame, bool hasMarker,
                             const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                             const std::shared_ptr<Buffer>& payload,
                             bool* configWasChanged) final;
    VideoFrameConfig GetVideoConfig(uint32_t ssrc) const final;
private:
    static bool ParseVp8VideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                    const std::shared_ptr<Buffer>& payload,
                                    VideoFrameConfig& applyTo);
    static bool ParseVp9VideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                    const std::shared_ptr<Buffer>& payload,
                                    VideoFrameConfig& applyTo);
private:
    std::unordered_map<uint32_t, std::unique_ptr<RtpAssembly>> _assemblies;
};

} // namespace RTC
