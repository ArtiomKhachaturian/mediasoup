#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class RtpDepacketizerVpx : public RtpVideoDepacketizer
{
    class RtpAssembly;
public:
    RtpDepacketizerVpx(uint32_t clockRate, bool vp8,
                       const std::shared_ptr<BufferAllocator>& allocator);
    ~RtpDepacketizerVpx() final;
    // impl. of RtpDepacketizer
    std::optional<MediaFrame> AddPacket(const RtpPacket* packet,
                                        bool makeDeepCopyOfPayload,
                                        bool* configWasChanged) final;
    VideoFrameConfig GetVideoConfig(const RtpPacket* packet) const final;
private:
    static bool ParseVp8VideoConfig(const RtpPacket* packet, VideoFrameConfig& applyTo);
    static bool ParseVp9VideoConfig(const RtpPacket* packet, VideoFrameConfig& applyTo);
private:
    absl::flat_hash_map<uint32_t, std::unique_ptr<RtpAssembly>> _assemblies;
};

} // namespace RTC
