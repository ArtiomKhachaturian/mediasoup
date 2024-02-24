#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class RtpDepacketizerVpx : public RtpDepacketizer
{
    class RtpAssembly;
public:
    RtpDepacketizerVpx(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                       const std::weak_ptr<BufferAllocator>& allocator);
    ~RtpDepacketizerVpx() final;
    // impl. of RtpDepacketizer
    std::shared_ptr<MediaFrame> AddPacket(const RtpPacket* packet,
                                          bool makeDeepCopyOfPayload) final;
private:
    absl::flat_hash_map<uint32_t, std::unique_ptr<RtpAssembly>> _assemblies;
};

} // namespace RTC
